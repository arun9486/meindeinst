#!/usr/bin/env python3

import os
import socket
import select
import ctypes
import errno
import struct

SOCKET_PATH = "/tmp/pid_input_socket"

libc = ctypes.CDLL("libc.so.6", use_errno=True)
SYS_pidfd_open = 434

def pidfd_open(pid, flags=0):
    res = libc.syscall(SYS_pidfd_open, pid, flags)
    if res == -1:
        e = ctypes.get_errno()
        raise OSError(e, os.strerror(e))
    return res

def process_exit_callback(pidfd):
    print(f"Callback: pidfd {pidfd} - Process exited")

def main():
    if os.path.exists(SOCKET_PATH):
        os.unlink(SOCKET_PATH)

    server_sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    server_sock.bind(SOCKET_PATH)
    server_sock.listen(5)
    server_sock.setblocking(False)

    epoll = select.epoll()
    epoll.register(server_sock.fileno(), select.EPOLLIN)

    fd_to_socket = {server_sock.fileno(): server_sock}
    
    while True:
      events = epoll.poll(-1)
      for fd, event in events:
          if fd == server_sock.fileno():
              conn, _ = server_sock.accept()
              conn.setblocking(False)
              epoll.register(conn.fileno(), select.EPOLLIN)
              fd_to_socket[conn.fileno()] = conn
              print("New connection accepted")
          elif event & select.EPOLLIN:
              if fd_to_socket.get(fd):
                  data = os.read(fd, 1024)
                  if not data:
                      epoll.unregister(fd)
                      fd_to_socket[fd].close()
                      del fd_to_socket[fd]
                      print("Client disconnected")
                  else:
                      lines = data.decode().splitlines()
                      for line in lines:
                          try:
                              pid = int(line.strip())
                              print(f"Received PID: {pid}")
                              pidfd = pidfd_open(pid)
                              epoll.register(pidfd, select.EPOLLIN)
                              fd_to_socket[pidfd] = None
                          except Exception as e:
                              print(f"Error processing PID '{line}': {e}")
              else:
                  process_exit_callback(fd)
                  epoll.unregister(fd)
                  os.close(fd)
                  del fd_to_socket[fd]


if __name__ == "__main__":
    main()
