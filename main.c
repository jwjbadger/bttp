#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>

int opensocket(int socktype, const char * service) {
    struct addrinfo hints = {0};
    struct addrinfo *saddrinfo;
    int sfd;

    hints.ai_socktype = socktype;
    hints.ai_family = AF_UNSPEC;
    hints.ai_flags = AI_PASSIVE;

    if (getaddrinfo(NULL, service, &hints, &saddrinfo) != 0) {
        perror("getaddrinfo");
        return -1;
    }

    for (struct addrinfo* n = saddrinfo; n != NULL; n = n->ai_next) {
        if ((sfd = socket(n->ai_family, n->ai_socktype, n->ai_protocol)) < 0) {
            perror("socket");
            continue;
        }

        if (setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) < 0) {
            perror("setsockopt");
            return -1;
        }

        if (bind(sfd, saddrinfo->ai_addr, saddrinfo->ai_addrlen) < 0) {
            close(sfd);
            perror("bind");
            sfd = -1;
            continue;
        }
        
        break;
    }

    freeaddrinfo(saddrinfo);

    if (sfd < 0) {
        fprintf(stderr, "Couldn't create / bind socket\n");
        return -1;
    }

    return sfd;
}

int main(int argc, char **argv) {
    int sfd = opensocket(SOCK_STREAM, "doom");

    if (listen(sfd, 3) < 0) {
        perror("listen");
        return -1;
    }

    struct sockaddr csaddr = {0};
    int csaddrlen = sizeof(csaddr);

    char buf[4096];

    printf("Accepting connections on port 666\n");
    while (1) {
        int cfd = accept(sfd, &csaddr, &csaddrlen); 
        if (cfd < 0) {
            perror("accept");
            return -1;
        }

        char caddrp[INET6_ADDRSTRLEN];
        if (inet_ntop(csaddr.sa_family, &((struct sockaddr_in *)&csaddr)->sin_addr, caddrp, INET6_ADDRSTRLEN) == NULL) {
            perror("inet_ntop");
            return -1;
        }
        printf("[%s] request using %u\n", caddrp, csaddr.sa_family);

        //if (recv(cfd, buf, 4096, 0) < 0) {
        //    perror("recv");
        //    return -1;
        //}
        //printf("%s\n", buf);

        const char * msg = "HTTP/1.0 200 OK\r\nContent-Type: text/html\r\n\r\n<html><body><h1>HELLO SHEEPLE</h1></body></html>";
        ssize_t sent = send(cfd, msg, strlen(msg), 0);
        if (sent < 0) {
            perror("send");
            return -1;
        } else if (sent != strlen(msg)) {
            fprintf(stderr, "Wasn't able to send entire message\n");
            return -1;
        }
        
        close(cfd);
    }
}
