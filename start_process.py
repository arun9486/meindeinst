import socket
import subprocess

SOCKET_PATH = "/tmp/pid_input_socket"

command = ["./sample.sh"]
process = subprocess.Popen(command)
print(f"The PID of the script is: {process.pid}")

with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as s:
    s.connect(SOCKET_PATH)
    pid_str = f"{process.pid}"
    s.sendall(pid_str.encode())