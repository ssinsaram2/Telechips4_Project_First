#!/usr/bin/env python3
import socket
import sys


def main() -> int:
    if len(sys.argv) < 3:
        print("usage: send_face_result.py <ui_or_rpi4_ip> <driver_id|0|FAIL> [port]", file=sys.stderr)
        return 2
    host = sys.argv[1]
    payload = sys.argv[2]
    port = int(sys.argv[3]) if len(sys.argv) >= 4 else 5000
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
        sock.sendto(payload.encode(), (host, port))
    print(f"sent face result {payload!r} to {host}:{port}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
