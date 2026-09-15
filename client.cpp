#include <arpa/inet.h>
#include <errno.h>
#include <iostream>
#include <netdb.h>
#include <sys/proc.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

int main() {
    struct sockaddr_storage their_addr;
    int sock_fd{}, new_fd{};
    struct addrinfo hints, *res;
    socklen_t addr_size{};

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    getaddrinfo(NULL, "3490", &hints, &res);
    sock_fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);

    if (errno != 0) {
        perror("Socket Creation Failed");
    }

    connect(sock_fd, res->ai_addr, res->ai_addrlen);
    if (errno != 0) {
        perror("Socket Connection Failed");
    }

    char buf[64];
    int numbytes{};
    if ((numbytes = recv(sock_fd, buf, 63, 0)) == -1) {
        perror("recv");
        exit(1);
    }

    buf[numbytes] = '\0';
    printf("Client recieved: %s", buf);

    close(sock_fd);
}
