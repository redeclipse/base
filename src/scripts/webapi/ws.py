# ws.py url [token] [seconds]: a minimal WebSocket client that prints what arrives, for testing the Event feed and relay
import socket, ssl, sys, os, base64, struct, time, json
from urllib.parse import urlparse
def connect(url, extra=""):
    u = urlparse(url)
    port = u.port or (443 if u.scheme == "wss" else 80)
    s = socket.create_connection((u.hostname, port), timeout=5)
    if u.scheme == "wss":
        ctx = ssl.create_default_context(); ctx.check_hostname = False; ctx.verify_mode = ssl.CERT_NONE
        s = ctx.wrap_socket(s, server_hostname=u.hostname)
    key = base64.b64encode(os.urandom(16)).decode()
    path = u.path + ("?" + u.query if u.query else "")
    s.sendall(("GET %s HTTP/1.1\r\nHost: %s\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Key: %s\r\nSec-WebSocket-Version: 13\r\n%s\r\n" % (path or "/", u.hostname, key, extra)).encode())
    head = b""
    while b"\r\n\r\n" not in head:
        d = s.recv(1)
        if not d: break
        head += d
    return s, head.decode(errors="replace")
def send(s, data, op=1):
    data = data.encode() if isinstance(data, str) else data
    mask = os.urandom(4)
    h = bytes([0x80 | op])
    n = len(data)
    if n < 126: h += bytes([0x80 | n])
    elif n < 65536: h += bytes([0x80 | 126]) + struct.pack(">H", n)
    else: h += bytes([0x80 | 127]) + struct.pack(">Q", n)
    s.sendall(h + mask + bytes(b ^ mask[i % 4] for i, b in enumerate(data)))
def recvexact(s, n):
    b = b""
    while len(b) < n:
        d = s.recv(n - len(b))
        if not d: raise EOFError
        b += d
    return b
def recv(s):
    h = recvexact(s, 2)
    op = h[0] & 15; n = h[1] & 127
    if n == 126: n = struct.unpack(">H", recvexact(s, 2))[0]
    elif n == 127: n = struct.unpack(">Q", recvexact(s, 8))[0]
    return op, recvexact(s, n)
if __name__ == "__main__":
    url = sys.argv[1]; token = sys.argv[2] if len(sys.argv) > 2 else ""; secs = float(sys.argv[3]) if len(sys.argv) > 3 else 3
    s, head = connect(url)
    print(head.split("\r\n")[0])
    if not head.startswith("HTTP/1.1 101"): print(head); sys.exit(1)
    # browsers fail the handshake without these, as RFC 6455 requires them
    for want in ("connection: upgrade", "upgrade: websocket"):
        if want not in head.lower(): print("missing %s" % want); print(head); sys.exit(1)
    if token: send(s, json.dumps({"token": token}))
    send(s, b"hi", 9)
    end = time.time() + secs
    s.settimeout(0.5)
    while time.time() < end:
        try:
            op, data = recv(s)
            print("op%d %s" % (op, data[:300].decode(errors="replace")))
        except socket.timeout: continue
        except EOFError: print("closed"); break
    send(s, struct.pack(">H", 1000), 8)
    try:
        op, data = recv(s); print("close reply op%d %r" % (op, data))
    except Exception as e: print("close", e)
