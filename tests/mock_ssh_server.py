import socket
import threading
import struct
import time

def handle_client(conn, addr):
    try:
        print(f"[Server] Connection from {addr}")

        # 1. Version Exchange
        client_ver = b""
        while not client_ver.endswith(b"\r\n"):
            chunk = conn.recv(1)
            if not chunk: return
            client_ver += chunk
        print(f"[Server] Client Version: {client_ver.strip().decode()}")

        # Send Server Version
        server_ver = b"SSH-2.0-MockServer_1.0\r\n"
        conn.sendall(server_ver)

        # 2. Handshake Loop (Packet Processing)
        while True:
            # Read Packet Header (4 byte len, 1 byte pad_len)
            header = b""
            while len(header) < 5:
                chunk = conn.recv(5 - len(header))
                if not chunk: return
                header += chunk

            pkt_len_field = struct.unpack(">I", header[0:4])[0]
            pad_len = header[4]

            # Read Payload + Padding (packet_len includes pad_len byte, which we read)
            remaining = pkt_len_field - 1
            body = b""
            while len(body) < remaining:
                chunk = conn.recv(remaining - len(body))
                if not chunk: return
                body += chunk

            # Extract Type
            msg_type = body[0]
            payload = body[1:len(body)-pad_len] # Strip padding

            print(f"[Server] Received Msg Type: {msg_type}")

            if msg_type == 20: # SSH_MSG_KEXINIT
                print("[Server] Sending SSH_MSG_KEXINIT")
                # Dummy KEXINIT: Cookie (16) + Lists (empty strings logic)
                # Client skeleton ignores content, so just send bytes
                # Cookie (16) + kex_algo (4+0) + hostkey (4+0) ...
                # Actually let's send just enough zero bytes to not crash generic parser if any
                send_packet(conn, 20, b"\x00"*16 + b"\x00"*100)

            elif msg_type == 21: # SSH_MSG_NEWKEYS
                print("[Server] Sending SSH_MSG_NEWKEYS")
                send_packet(conn, 21, b"")

            elif msg_type == 5: # SSH_MSG_SERVICE_REQUEST
                print("[Server] Sending SSH_MSG_SERVICE_ACCEPT")
                svc = payload[4:]
                # 6 = SERVICE_ACCEPT
                # Payload: string service_name
                # string "ssh-userauth" = \x00\x00\x00\x0c ssh-userauth
                send_packet(conn, 6, b"\x00\x00\x00\x0c" + b"ssh-userauth")

            elif msg_type == 50: # SSH_MSG_USERAUTH_REQUEST
                # Check probe vs sign
                if b"dummy_pubkey_probe" in payload:
                    print("[Server] Sending SSH_MSG_USERAUTH_PK_OK")
                    # 60 = PK_OK
                    # Payload: string algo, string blob
                    # Mock: algo="ssh-ed25519", blob=""
                    send_packet(conn, 60, b"\x00\x00\x00\x0bssh-ed25519" + b"\x00\x00\x00\x00")
                elif b"dummy_signed_request" in payload:
                    print("[Server] Sending SSH_MSG_USERAUTH_SUCCESS")
                    send_packet(conn, 52, b"") # SUCCESS
                else:
                    print(f"[Server] Unknown Auth Request Payload: {payload}")
                    # Default to Success to not block other tests? No, explicit fail
                    # send_packet(conn, 51, b"") # FAILURE

            elif msg_type == 90: # SSH_MSG_CHANNEL_OPEN
                print("[Server] Sending SSH_MSG_CHANNEL_OPEN_CONFIRMATION")
                # 91 = OPEN_CONFIRMATION
                # uint32 recipient channel, uint32 sender channel, uint32 init window, uint32 max packet
                resp = struct.pack(">IIII", 0, 0, 32768, 32768)
                send_packet(conn, 91, resp)

            elif msg_type == 98: # SSH_MSG_CHANNEL_REQUEST (e.g. PTY, SHELL)
                print("[Server] Sending SSH_MSG_CHANNEL_SUCCESS")
                # 99 = SUCCESS
                # uint32 recipient channel
                resp = struct.pack(">I", 0)
                send_packet(conn, 99, resp)

            elif msg_type == 94: # SSH_MSG_CHANNEL_DATA
                # Echo data back
                data = payload[4:] # Skip recipient channel (4 bytes)
                print(f"[Server] Echoing Data: {data}")
                # 94 = DATA
                # uint32 recipient, string data
                resp = struct.pack(">I", 0) + struct.pack(">I", len(data)) + data
                send_packet(conn, 94, resp)

            elif msg_type == 1: # DISCONNECT
                print("[Server] Client Disconnected")
                return

    except Exception as e:
        print(f"[Server] Error: {e}")
    finally:
        conn.close()

def send_packet(conn, msg_type, payload):
    pad_len = 4
    # packet_len = len(pad_len_byte + msg_type_byte + payload + padding)
    #            = 1 + 1 + len(payload) + pad_len
    pkt_len = 1 + 1 + len(payload) + pad_len

    header = struct.pack(">IB", pkt_len, pad_len)
    # msg_type is byte
    msg = header + bytes([msg_type]) + payload + (b"\x00" * pad_len)
    conn.sendall(msg)

def main():
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    s.bind(('127.0.0.1', 2222))
    s.listen(1)
    print("[Server] Listening on 127.0.0.1:2222...", flush=True)

    while True:
        conn, addr = s.accept()
        t = threading.Thread(target=handle_client, args=(conn, addr))
        t.start()

if __name__ == "__main__":
    main()
