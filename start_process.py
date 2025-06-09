import socket

SOCKET_PATH = "/tmp/pid_input_socket"

with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as s:
    s.connect(SOCKET_PATH)
    s.sendall(b"12345\n")
