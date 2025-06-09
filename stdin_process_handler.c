/* #include <sys/types.h> */
/* #include <sys/epoll.h> */
/* #include <sys/syscall.h> */
/* #include <unistd.h> */
/* #include <stdio.h> */
/* #include <stdlib.h> */

/* void process_exit_callback(pid_t pid) { */
/*     printf("Callback: Process %d exited!\n", pid); */
/* } */

/* int main(int argc, char *argv[]) { */
/*     if (argc != 2) { */
/*         printf("Usage: %s <pid>\n", argv[0]); */
/*         exit(1); */
/*     } */

/*     pid_t pid = atoi(argv[1]); */

/*     // Open pidfd for the external PID */
/*     int pidfd = syscall(SYS_pidfd_open, pid, 0); */
/*     if (pidfd == -1) { */
/*         perror("pidfd_open"); */
/*         exit(1); */
/*     } */

/*     // Setup epoll */
/*     int efd = epoll_create1(0); */
/*     struct epoll_event ev = { .events = EPOLLIN, .data.fd = pidfd }; */
/*     epoll_ctl(efd, EPOLL_CTL_ADD, pidfd, &ev); */

/*     printf("Monitoring external process PID: %d\n", pid); */

/*     // Wait for exit event */
/*     struct epoll_event events[1]; */
/*     epoll_wait(efd, events, 1, -1); */

/*     process_exit_callback(pid); */

/*     close(pidfd); */
/*     close(efd); */
/*     return 0; */
/* } */


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <sys/syscall.h>

#define MAX_EVENTS 10

// not required for latest kernal version
/* #define SYS_pidfd_open 434  // Verify for your kernel version */

void process_exit_callback(pid_t pid) {
    printf("Process %d exited\n", pid);
}

int main() {
    int efd = epoll_create1(0);
    if (efd == -1) {
        perror("epoll_create1");
        exit(1);
    }

    // Add stdin to epoll to receive new PIDs
    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = STDIN_FILENO;
    if (epoll_ctl(efd, EPOLL_CTL_ADD, STDIN_FILENO, &ev) == -1) {
        perror("epoll_ctl");
        close(efd);
        exit(1);
    }

    printf("Enter PIDs to monitor (one per line):\n");

    while (1) {
        struct epoll_event events[MAX_EVENTS];
        int nfds = epoll_wait(efd, events, MAX_EVENTS, -1);
        if (nfds == -1) {
            perror("epoll_wait");
            continue;
        }

        for (int i = 0; i < nfds; i++) {
            if (events[i].data.fd == STDIN_FILENO) {
                // Handle new PID input
                char buffer[1024];
                ssize_t bytes = read(STDIN_FILENO, buffer, sizeof(buffer));
                if (bytes > 0) {
                    pid_t pid;
                    if (sscanf(buffer, "%d", &pid) == 1) {
                        int pidfd = syscall(SYS_pidfd_open, pid, 0);
                        if (pidfd == -1) {
                            perror("pidfd_open");
                            continue;
                        }
                        
                        struct epoll_event new_ev;
                        new_ev.events = EPOLLIN;
                        new_ev.data.fd = pidfd;
                        if (epoll_ctl(efd, EPOLL_CTL_ADD, pidfd, &new_ev) == -1) {
                            perror("epoll_ctl");
                            close(pidfd);
                            continue;
                        }
                        printf("Now monitoring PID: %d\n", pid);
                    }
                }
            } else {
                // Handle process exit
                int pidfd = events[i].data.fd;
                process_exit_callback(pidfd);  // Note: Actual PID retrieval needs implementation
                epoll_ctl(efd, EPOLL_CTL_DEL, pidfd, NULL);
                close(pidfd);
            }
        }
    }
    
    close(efd);
    return 0;
}
