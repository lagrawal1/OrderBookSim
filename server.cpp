#include <arpa/inet.h>
#include <errno.h>
#include <iostream>
#include <netdb.h>
#include <signal.h>
#include <string>
#include <sys/proc.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

void serialize_data(void *data, size_t size, int offset, uint8_t *buffer) {
    memcpy(buffer + offset, data, size);
}

void sigchld_handler(int s) {
    (void)s;
    // waitpid can modify errno, so it must be saved and restore
    int saved_errno = errno;

    while (waitpid(-1, NULL, WNOHANG) > 0)
        ;

    errno = saved_errno;
}

int main() {
    struct sockaddr_storage client_addr;
    int sock_fd{}, client_fd{};
    struct addrinfo hints, *servinfo_ll, *res;

    socklen_t client_addr_size{};
    struct sigaction sa;
    int yes = 1;
    int gai_rv;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    if ((gai_rv = getaddrinfo(NULL, "3490", &hints, &servinfo_ll)) != 0) {
        fprintf(stderr, "gaierror: %s", gai_strerror(gai_rv));
    };

    for (res = servinfo_ll; res != NULL; res = res->ai_next) {
        if ((sock_fd = socket(res->ai_family, res->ai_socktype,
                              res->ai_protocol)) == -1) {
            perror("Socket Creation Failed");
            continue;
        };

        if (setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) ==
            -1) {
            perror("setsockopt");
            exit(1);
        }

        if (bind(sock_fd, res->ai_addr, res->ai_addrlen) == -1) {
            perror("Socket Binding Failed");
            continue;
        };

        break;
    }

    if (res == NULL) {
        fprintf(stderr, "socket failed to bind");
        exit(1);
    }

    freeaddrinfo(servinfo_ll);

    if (listen(sock_fd, 2) != 0) {
        perror("listen");
        exit(1);
    };

    // reaping zombie processes

    sa.sa_handler = &sigchld_handler; // Assign sigaction handler
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    // Register using sigaction to call sigchld_handler when sigchld (child
    // process terminates, restarts, etc.)
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("Sigaction Error: ");
        exit(1);
    }
    std::cout << "Welcome to the server\n";

    while (true) {

        client_addr_size = sizeof(client_addr);
        client_fd = accept(sock_fd, (struct sockaddr *)&client_addr,
                           &client_addr_size); // New socket for connection

        if (client_fd == -1) {
            perror("accept");
            continue;
        }
        std::string msg{};
        if (!fork()) {
            close(sock_fd); // Close original socket
            std::cout << "Please enter your msg to send: ";

            std::getline(std::cin, msg);

            if (send(client_fd, msg.c_str(), msg.length() + 1, 0) == -1) {
                perror("send");
            }
            close(client_fd);
            exit(0);
        }
    }
    return 0;
}
