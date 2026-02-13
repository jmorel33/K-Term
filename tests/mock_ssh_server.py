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
                send_packet(conn, 20, b"\x00"*16 + b"\x00"*100)

            elif msg_type == 21: # SSH_MSG_NEWKEYS
                print("[Server] Sending SSH_MSG_NEWKEYS")
                send_packet(conn, 21, b"")

            elif msg_type == 5: # SSH_MSG_SERVICE_REQUEST
                print("[Server] Sending SSH_MSG_SERVICE_ACCEPT")
                send_packet(conn, 6, b"\x00\x00\x00\x0c" + b"ssh-userauth")

            elif msg_type == 50: # SSH_MSG_USERAUTH_REQUEST
                if b"dummy_pubkey_probe" in payload:
                    print("[Server] Sending SSH_MSG_USERAUTH_PK_OK")
                    send_packet(conn, 60, b"\x00\x00\x00\x0bssh-ed25519" + b"\x00\x00\x00\x00")
                elif b"dummy_signed_request" in payload:
                    print("[Server] Sending SSH_MSG_USERAUTH_SUCCESS")
                    send_packet(conn, 52, b"") # SUCCESS
                else:
                    # Assume password auth success
                    print(f"[Server] Auth Request Payload: {payload[:20]}...")
                    print("[Server] Sending SSH_MSG_USERAUTH_SUCCESS")
                    send_packet(conn, 52, b"")

            elif msg_type == 90: # SSH_MSG_CHANNEL_OPEN
                sender_chan = struct.unpack(">I", payload[4:8])[0]
                print(f"[Server] SSH_MSG_CHANNEL_OPEN Sender ID: {sender_chan}")
                print("[Server] Sending SSH_MSG_CHANNEL_OPEN_CONFIRMATION")
                # 91 = OPEN_CONFIRMATION
                # recipient (sender_chan), sender (our ID=sender_chan to map 1:1), window, packet
                resp = struct.pack(">IIII", sender_chan, sender_chan, 32768, 32768)
                send_packet(conn, 91, resp)

            elif msg_type == 98: # SSH_MSG_CHANNEL_REQUEST
                recipient = struct.unpack(">I", payload[0:4])[0]
                req_len = struct.unpack(">I", payload[4:8])[0]
                req_type = payload[8:8+req_len].decode('ascii')
                want_reply = payload[8+req_len]

                print(f"[Server] Channel Request: {req_type} on Channel {recipient}")

                if want_reply:
                    print("[Server] Sending SSH_MSG_CHANNEL_SUCCESS")
                    resp = struct.pack(">I", recipient)
                    send_packet(conn, 99, resp)

                if req_type == "exec":
                    print("[Server] Exec detected. Sending EXIT_STATUS and CLOSE")
                    # 98 = REQUEST (exit-status)
                    # We can send request on channel too? No, usually exit-status is a request from server to client.
                    # recipient, "exit-status", false, uint32 code
                    req_s = b"exit-status"
                    # req payload: recipient(4) + len(4) + str + want_reply(1) + code(4)
                    exit_load = struct.pack(">I", recipient) + struct.pack(">I", len(req_s)) + req_s + b"\x00" + struct.pack(">I", 0)
                    send_packet(conn, 98, exit_load)

                    # 97 = CHANNEL_CLOSE
                    resp = struct.pack(">I", recipient)
                    send_packet(conn, 97, resp)

            elif msg_type == 94: # SSH_MSG_CHANNEL_DATA
                recipient = struct.unpack(">I", payload[0:4])[0]
                data = payload[8:] # Skip recip(4) + len(4)
                print(f"[Server] Echoing Data on Channel {recipient}: {data}")
                resp = struct.pack(">I", recipient) + struct.pack(">I", len(data)) + data
                send_packet(conn, 94, resp)

            elif msg_type == 97: # SSH_MSG_CHANNEL_CLOSE
                print("[Server] Received CHANNEL_CLOSE")
                # Acknowledge if needed? Usually symmetric close.
                # If client initiates close (e.g. after exec finish), we are done with that channel.
                pass

            elif msg_type == 1: # DISCONNECT
                print("[Server] Client Disconnected")
                return

    except Exception as e:
        print(f"[Server] Error: {e}")
    finally:
        conn.close()

def send_packet(conn, msg_type, payload):
    pad_len = 4
    pkt_len = 1 + 1 + len(payload) + pad_len
    header = struct.pack(">IB", pkt_len, pad_len)
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
