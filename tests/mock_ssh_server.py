import socket
import threading
import struct
import time

def handle_client(conn, addr):
    print(f"[Server] Connection from {addr}")

    # 1. Version Exchange
    # Read Client Version
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

        pkt_len = struct.unpack(">I", header[0:4])[0]
        pad_len = header[4]

        # Read Payload + Padding
        remaining = pkt_len
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
            # Respond with KEXINIT (Dummy)
            # Just send same packet back for mock?
            # Or send minimal valid structure.
            # Skeleton doesn't parse it deeply, so send dummy KEXINIT
            print("[Server] Sending SSH_MSG_KEXINIT")
            send_packet(conn, 20, b"\x00"*16 + b"\x00"*100) # Cookie + empty lists

            # Expect NEWKEYS next?
            # Skeleton sends NEWKEYS immediately after receiving KEXINIT? No, it sends KEXINIT then waits.
            # Client State: KEX_INIT -> sends KEXINIT -> waits for KEXINIT -> sends NEWKEYS

        elif msg_type == 21: # SSH_MSG_NEWKEYS
            print("[Server] Sending SSH_MSG_NEWKEYS")
            send_packet(conn, 21, b"")

        elif msg_type == 5: # SSH_MSG_SERVICE_REQUEST
            print("[Server] Sending SSH_MSG_SERVICE_ACCEPT")
            # Payload: string service_name
            svc = payload[4:] # skip length prefix (4 bytes) - wait, string is [len:4][data]
            # Skeleton sends "ssh-userauth"
            send_packet(conn, 6, b"\x00\x00\x00\x0c" + b"ssh-userauth")

        elif msg_type == 50: # SSH_MSG_USERAUTH_REQUEST
            print("[Server] Sending SSH_MSG_USERAUTH_SUCCESS")
            send_packet(conn, 52, b"") # SUCCESS

        elif msg_type == 90: # SSH_MSG_CHANNEL_OPEN
            print("[Server] Sending SSH_MSG_CHANNEL_OPEN_CONFIRMATION")
            # uint32 recipient channel, uint32 sender channel, uint32 init window, uint32 max packet
            # Skeleton sends "session"
            # Response: 91
            resp = struct.pack(">IIII", 0, 0, 32768, 32768)
            send_packet(conn, 91, resp)

        elif msg_type == 98: # SSH_MSG_CHANNEL_REQUEST (e.g. PTY, SHELL)
            # Skeleton sends PTY then SHELL
            # We just say OK
            print("[Server] Sending SSH_MSG_CHANNEL_SUCCESS")
            # uint32 recipient channel
            resp = struct.pack(">I", 0)
            send_packet(conn, 99, resp) # SUCCESS

        elif msg_type == 94: # SSH_MSG_CHANNEL_DATA
            # Echo data back
            # uint32 recipient, string data
            data_len = struct.unpack(">I", payload[4:8])[0]
            data = payload[8:8+data_len]
            print(f"[Server] Echoing Data: {data}")

            # Send back
            resp = struct.pack(">I", 0) + struct.pack(">I", len(data)) + data
            send_packet(conn, 94, resp)

def send_packet(conn, msg_type, payload):
    pad_len = 4
    total_len = 1 + len(payload) + pad_len + 1 # len(1) + type(1) + payload + pad_len(1) + pad
    # RFC: packet_len = len(pad_len_byte + payload + padding)
    #      packet_len does NOT include packet_len field itself (4 bytes)

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
