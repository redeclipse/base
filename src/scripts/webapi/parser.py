# parser.py [port]: throws malformed and awkward requests at a local Web API and prints the status lines that come back
import socket, sys, time
port = int(sys.argv[1]) if len(sys.argv) > 1 else 28888
def send(data, wait=1.0, chunks=None):
    s = socket.create_connection(("127.0.0.1", port), timeout=3)
    try:
        if chunks:
            for c in chunks: s.sendall(c); time.sleep(0.05)
        else: s.sendall(data)
        s.settimeout(wait)
        out = b""
        try:
            while True:
                d = s.recv(65536)
                if not d: break
                out += d
        except socket.timeout: pass
        return out
    except Exception as e:
        return ("EXC %s" % e).encode()
    finally: s.close()
def status(r): 
    lines = r.split(b"\r\n")
    return [l.decode(errors="replace") for l in lines if l.startswith(b"HTTP/")]
tests = {
 "garbage": b"\x00\x01\x02 hello\r\n\r\n",
 "badversion": b"GET / HTTP/2.0\r\n\r\n",
 "unknown method": b"BREW / HTTP/1.1\r\nHost: x\r\n\r\n",
 "no target": b"GET  HTTP/1.1\r\n\r\n",
 "relative target": b"GET foo HTTP/1.1\r\n\r\n",
 "long line": b"GET /" + b"a"*20000 + b" HTTP/1.1\r\n\r\n",
 "many headers": b"GET / HTTP/1.1\r\n" + b"".join(b"X-%d: y\r\n" % i for i in range(200)) + b"\r\n",
 "header no colon": b"GET / HTTP/1.1\r\nBroken\r\n\r\n",
 "folded header": b"GET / HTTP/1.1\r\nA: b\r\n c\r\n\r\n",
 "pipelined": b"GET /a HTTP/1.1\r\n\r\nGET /b HTTP/1.1\r\n\r\nGET /c HTTP/1.1\r\nConnection: close\r\n\r\n",
 "chunked body": b"POST /x HTTP/1.1\r\nTransfer-Encoding: chunked\r\n\r\n5\r\nhello\r\n0\r\n\r\n",
 "bad chunk": b"POST /x HTTP/1.1\r\nTransfer-Encoding: chunked\r\n\r\nzz\r\nhello\r\n0\r\n\r\n",
 "huge length": b"POST /x HTTP/1.1\r\nContent-Length: 999999999999\r\n\r\n",
 "neg length": b"POST /x HTTP/1.1\r\nContent-Length: -5\r\n\r\n",
 "both te and cl": b"POST /x HTTP/1.1\r\nContent-Length: 3\r\nTransfer-Encoding: chunked\r\n\r\n0\r\n\r\n",
 "te gzip only": b"POST /x HTTP/1.1\r\nTransfer-Encoding: gzip\r\n\r\n",
 "http10 close": b"GET / HTTP/1.0\r\n\r\n",
 "expect continue": b"POST /x HTTP/1.1\r\nContent-Length: 2\r\nExpect: 100-continue\r\n\r\nhi",
 "ws no key": b"GET /x HTTP/1.1\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n\r\n",
 "split slowly": None,
}
for name, data in tests.items():
    if data is None:
        r = send(None, chunks=[b"GE", b"T /s", b"low HTTP/1.1\r", b"\nHost: a\r\n", b"\r\n"])
    else:
        r = send(data)
    print("%-16s %s" % (name, status(r) if r and not r.startswith(b"EXC") else r[:60]))
