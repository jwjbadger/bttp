#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

typedef struct http_header {
    char *key;
    char *value;
    struct http_header *next; // Should there be a back pointer too?
} http_header;

typedef struct http_req {
    char *method;
    char *path;
    char *version;
    http_header *headers;
    char *body; // Not even gonna try to parse this...
} http_req;

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

int parse_req(const char *req, http_req *freq) {
    char *method = strchr(req, ' ');
    if (method == NULL) {
        fprintf(stderr, "Error parsing HTTP method\n");
        return -1;
    }

    freq->method = malloc(1 + method - req);
    strncpy(freq->method, req, method - req);
    freq->method[method - req] = 0;

    char *path = strchr(++method, ' ');
    if (path == NULL) {
        free(freq->method);
        fprintf(stderr, "Error parsing HTTP path\n");
        return -1;
    }

    freq->path = malloc(1 + path - method);
    strncpy(freq->path, method, path - method);
    freq->path[path - method] = 0; 

    char *version = strchr(++path, '\r');
    if (version == NULL) {
        free(freq->method);
        free(freq->path);
        fprintf(stderr, "Error parsing HTTP version\n");
        return -1;
    }

    freq->version = malloc(1 + version - path - 5); // 5 represents the length of "HTTP/"
    strncpy(freq->version, path + 5, version - path - 5);
    freq->version[version - path - 5] = 0;

    freq->headers = malloc(sizeof(http_header));
    char *index = strchr(version, '\n');
    http_header *head = freq->headers;

    while (1) {
        char *end = strchr(++index, ':');
        if (end == NULL) {
            fprintf(stderr, "Error parsing key for msg:\n%s", index);
            return -1;
        }

        head->key = malloc(1 + end - index);
        strncpy(head->key, index, end - index);
        head->key[end - index] = 0;

        index = end + 2;
        end = strchr(index, '\r');
        if (end == NULL && (end = strchr(index, '\n')) == NULL) {
            fprintf(stderr, "Error parsing value for key: %s\n", head->key);
            return -1;
        }

        head->value = malloc(1 + end - index);
        strncpy(head->value, index, end - index);
        head->value[end - index] = 0;

        index = strchr(index, '\n');

        if (index == NULL || index[1] == '\r' || index[1] == '\n') {
            head->next = NULL;
            break;
        }

        head->next = malloc(sizeof(http_header));
        head = head->next;
    }

    if (index == NULL) {
        return 0;
    }

    freq->body = "What's the point?"; // PARSE BODY
    return 0; 
}

void freereq(http_req *req) {
    free(req->method);
    free(req->path);
    free(req->version);

    for (http_header *head = req->headers->next, *prev = req->headers; head != NULL; head = head->next) {
        free(head->key);
        free(head->value);

        free(prev);
        prev = head;
    }
}

int main(int argc, char **argv) {
    int sfd = opensocket(SOCK_STREAM, "doom");
    if (sfd < 0) {
        fprintf(stderr, "Failed to open socket\n");
        return -1;
    }

    if (listen(sfd, 3) < 0) {
        perror("listen");
        return -1;
    }

    struct sockaddr csaddr = {0};
    socklen_t csaddrlen = sizeof(csaddr);

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
        printf("[%s] request using %u\n\n", caddrp, csaddr.sa_family);

        ssize_t rlen;
        if ((rlen = recv(cfd, buf, 4096, 0)) < 0) {
            perror("recv");
            return -1;
        }
        buf[rlen] = 0;
        printf(buf);
        
        http_req req = {0}; 
        if (parse_req(buf, &req) < 0) {
            fprintf(stderr, "Failed to parse request\n");

            char *msg = "HTTP/1.0 400 Bad Request";
            send(cfd, msg, strlen(msg), 0); // If we can't send this we're probably ok? should we even error?
            close(cfd);

            continue;
        }
        printf("Parsed req method [%s] at path ['%s'] using HTTP version [%s]\n\n", req.method, req.path, req.version);

        if (strcmp(req.method, "GET") != 0) {
            char *msg = "HTTP/1.0 501 Not Implemented\r\nContent-Type: text/html\r\n\r\n<html><body><h1>501 Not Implemented</h1></body></html>";
            send(cfd, msg, strlen(msg), 0);
            close(cfd);
            
            continue;
        }

        char *msg = "HTTP/1.0 200 OK\r\nContent-Type: text/html\r\n\r\n<html><body><h1>CONGRATS, YOU SUCCESSFULLY GOT '%s'</h1></body></html>";
        char *fmsg = malloc(1 + strlen(msg) + strlen(req.path));
        snprintf(fmsg, 1 + strlen(msg) + strlen(req.path), msg, req.path);

        ssize_t sent = send(cfd, fmsg, strlen(fmsg), 0);
        if (sent < 0) {
            perror("send");
            return -1;
        } else if (sent != strlen(fmsg)) {
            fprintf(stderr, "Wasn't able to send entire message\n");
            return -1;
        }
        
        close(cfd);
        freereq(&req);
        free(fmsg);
    }
}
