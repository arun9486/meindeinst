#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <sys/syscall.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <string.h>
#include <errno.h>

#define MAX_EVENTS 10
#define SOCKET_PATH "/tmp/pid_input_socket"

void process_exit_callback(pid_t pid) {
    printf("Process %d exited\n", pid);
}

int main() {
    // Remove any previous socket file
    unlink(SOCKET_PATH);

    // Create UNIX domain socket
    int listen_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (listen_fd == -1) {
        perror("socket");
        exit(1);
    }

    struct sockaddr_un addr = {0};
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);

    if (bind(listen_fd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        perror("bind");
        close(listen_fd);
        exit(1);
    }

    if (listen(listen_fd, 5) == -1) {
        perror("listen");
        close(listen_fd);
        exit(1);
    }

    // Setup epoll
    int efd = epoll_create1(0);
    if (efd == -1) {
        perror("epoll_create1");
        close(listen_fd);
        exit(1);
    }

    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = listen_fd;
    if (epoll_ctl(efd, EPOLL_CTL_ADD, listen_fd, &ev) == -1) {
        perror("epoll_ctl");
        close(efd);
        close(listen_fd);
        exit(1);
    }

    printf("Waiting for PIDs via UNIX socket: %s\n", SOCKET_PATH);

    while (1) {
        struct epoll_event events[MAX_EVENTS];
        int nfds = epoll_wait(efd, events, MAX_EVENTS, -1);
        if (nfds == -1) {
            perror("epoll_wait");
            continue;
        }

        for (int i = 0; i < nfds; i++) {
            if (events[i].data.fd == listen_fd) {
                // Accept new connection
                int conn_fd = accept(listen_fd, NULL, NULL);
                if (conn_fd == -1) {
                    perror("accept");
                    continue;
                }
                // Add new connection to epoll
                struct epoll_event conn_ev;
                conn_ev.events = EPOLLIN;
                conn_ev.data.fd = conn_fd;
                if (epoll_ctl(efd, EPOLL_CTL_ADD, conn_fd, &conn_ev) == -1) {
                    perror("epoll_ctl add conn_fd");
                    close(conn_fd);
                }
            } else {
                // Handle data from client or process exit event
                char buffer[1024];
                ssize_t bytes = read(events[i].data.fd, buffer, sizeof(buffer) - 1);
                if (bytes > 0) {
                    buffer[bytes] = '\0';
                    char *line = buffer;
                    while (line < buffer + bytes) {
                        pid_t pid;
                        if (sscanf(line, "%d", &pid) == 1) {
                            int pidfd = syscall(SYS_pidfd_open, pid, 0);
                            if (pidfd == -1) {
                                perror("pidfd_open");
                            } else {
                                struct epoll_event new_ev;
                                new_ev.events = EPOLLIN;
                                new_ev.data.fd = pidfd;
                                if (epoll_ctl(efd, EPOLL_CTL_ADD, pidfd, &new_ev) == -1) {
                                    perror("epoll_ctl add pidfd");
                                    close(pidfd);
                                } else {
                                    printf("Now monitoring PID: %d\n", pid);
                                }
                            }
                        }
                        while (*line != '\n' && line < buffer + bytes) line++;
                        if (*line == '\n') line++;
                    }
                } else if (bytes == 0) {
                    // Connection closed
                    epoll_ctl(efd, EPOLL_CTL_DEL, events[i].data.fd, NULL);
                    close(events[i].data.fd);
                } else {
                    // Could be a process exit event (pidfd)
                    int fd = events[i].data.fd;
                    printf("A monitored process has exited (pidfd=%d)\n", fd);
                    process_exit_callback(fd);
                    epoll_ctl(efd, EPOLL_CTL_DEL, fd, NULL);
                    close(fd);
                }
            }
        }
    }

    close(efd);
    close(listen_fd);
    unlink(SOCKET_PATH);
    return 0;
}
