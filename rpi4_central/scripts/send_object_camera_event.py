#!/usr/bin/env python3
import socket
import sys


def main() -> int:
    if len(sys.argv) < 3:
        print("usage: send_object_camera_event.py <rpi4_ip> <0|1> [port]", file=sys.stderr)
        return 2
    host = sys.argv[1]
    detected = "1" if sys.argv[2] not in ("0", "false", "False", "off") else "0"
    port = int(sys.argv[3]) if len(sys.argv) >= 4 else 5010
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
        sock.sendto(f"PERSON={detected}".encode(), (host, port))
    print(f"sent object camera event PERSON={detected} to {host}:{port}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
