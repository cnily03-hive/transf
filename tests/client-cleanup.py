import socket
import sys

host = "127.0.0.1"
port = 3081
TIMEOUT = 2


def parse_args():
    global host, port
    if len(sys.argv) == 2:
        host = sys.argv[1]
    elif len(sys.argv) == 3:
        host = sys.argv[1]
        port = int(sys.argv[2])


def print_sent(sent):
    print("\033[96m[SND]\033[0m", sent)


def print_recv(recv):
    print("\033[92m[RCV]\033[0m", recv)


if __name__ == "__main__":

    parse_args()

    remote = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    remote.connect((host, port))
    remote.settimeout(TIMEOUT)

    remote.send(b"\013HELLO")
    data = remote.recv(1024)
    print_sent(data)

    remote.send(b"\013HSxxxxxxxxxxxxxxxxxxxxxxxxxxx")
    data = remote.recv(1024)
    print_recv(data)
