#!/usr/bin/env python3

import argparse
import socket

VALID_COMMANDS = {"HALT", "OPEN", "CLOSE"}
EXIT_COMMANDS = {"Q", "QUIT", "EXIT"}


def send_command(sock: socket.socket, host: str, port: int, command: str) -> None:
    sock.sendto(command.encode("ascii"), (host, port))
    print(f"[control] sent {command} -> {host}:{port}")


def run_console(host: str, port: int) -> int:
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
        print(f"[control] UDP target {host}:{port}")
        print("[control] type HALT, OPEN, CLOSE, or quit")

        while True:
            try:
                text = input("slipstream-control> ")
            except (EOFError, KeyboardInterrupt):
                print()
                return 0

            command = text.strip().upper()
            if not command:
                continue
            if command in EXIT_COMMANDS:
                return 0
            if command not in VALID_COMMANDS:
                print("[control] invalid command; use HALT, OPEN, CLOSE, or quit")
                continue

            send_command(sock, host, port, command)


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Interactive UDP control console for Slipstream session commands."
    )
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=9099)
    args = parser.parse_args()

    return run_console(args.host, args.port)


if __name__ == "__main__":
    raise SystemExit(main())