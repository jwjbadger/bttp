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

const char* SERVER_DIR = "./serve";

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

typedef struct http_res {
    char *version;
    char *status;
    http_header *headers;
    char *body;
} http_res;


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
        } // do you have to put the OK after 200 http

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

    if (strstr(freq->path, "..") != NULL) {
        free(freq->method);
        free(freq->path);
        return -1;
    }

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

    char *index = strchr(version, '\n');
    if (index[1] == 0 || index == NULL) {
        freq->headers = NULL;
        return 1; // no header
    }

    freq->headers = malloc(sizeof(http_header));
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

void freeheaders(http_header *headers) {
    for (http_header *head = headers, *prev; head != NULL; head = head->next, free(prev)) {
        free(head->key);
        free(head->value);

        prev = head;
    }
}

void freereq(http_req *req) {
    free(req->method);
    free(req->path);
    free(req->version);
    freeheaders(req->headers);
}

void freeres(http_res *res) {
    free(res->body);
    // freeheaders(res->headers);
}

char *serialize_res(http_res *res) {
    ssize_t headerlen = 0;
    for (http_header *head = res->headers; head != NULL; head = head->next) {
        headerlen += 4 + strlen(head->key) + strlen(head->value);
    }

    char* headers = malloc(headerlen + 1);
    ssize_t index = 0;

    for (http_header *head = res->headers; head != NULL; head = head->next) {
        index += snprintf(headers + index, 1 + headerlen - index, "%s: %s\r\n", head->key, head->value); // if this actually works it's awful; side-note snprintf DOES null-terminate
    }

    ssize_t msglen = 10 + strlen(res->version) + strlen(res->status) + headerlen + strlen(res->body);
    char *msg = malloc(msglen + 1);

    snprintf(msg, msglen + 1, "HTTP/%s %s\r\n%s\r\n%s", res->version, res->status, headers, res->body);

    free(headers);
    return msg;
}

// Everything on res should be filled out except body
int sendfile(int cfd, http_res *res, char *path) {
    FILE *resf = fopen(path, "r");

    if (resf == NULL) {
        return -2;
    }

    fseek(resf, 0, SEEK_END);
    long fsize = ftell(resf);
    fseek(resf, 0, SEEK_SET);

    char *contents = malloc(fsize + 1);
    fread(contents, fsize, 1, resf);
    contents[fsize] = 0;
    
    fclose(resf);

    res->body = contents; 

    char *msg = serialize_res(res);

    ssize_t sent = send(cfd, msg, strlen(msg), 0);
    freeres(res);

    if (sent < 0) {
        free(msg);
        perror("send");
        return -1;
    } else if (sent != strlen(msg)) {
        free(msg);
        fprintf(stderr, "Wasn't able to send entire message\n");
        return -1;
    }

    free(msg);

    return 0;
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

        if (fork() != 0) {
            close(cfd); // Main process doesn't need client info
            continue; // Accept a new request
        }

        close(sfd); // We don't need any info about the server as the child process

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
        printf("%s", buf);
        
        http_req req = {0};

        char *status = "200 OK";
        char *path;

        http_header head = {"Content-Type", "text/plain"};
        http_res res = {"1.1", status, &head, NULL};

        if (parse_req(buf, &req) < 0) {
            fprintf(stderr, "Failed to parse request\n");

            // Even though we know these at compile time we malloc them so they can be freed at the end without error
            // I'm honestly not sure what the best way to handle this would be
            // Should we just send everything here and `continue`?
            req.method = malloc(4);
            req.headers = NULL;
            req.path = malloc(10);
            req.version = malloc(4);

            strcpy(req.method, "GET");
            strcpy(req.path, "/404.html");
            strcpy(req.version, "1.0");

            req.method[3] = 0;
            req.path[9] = 0;
            req.version[3] = 0;

            status = "400 Bad Request";

            path = malloc(strlen(SERVER_DIR) + 10);
            strcpy(path, SERVER_DIR);
            strcpy(path + strlen(SERVER_DIR), "/400.html");
            path[strlen(SERVER_DIR) + 9] = 0;
        }
        printf("Parsed req method [%s] at path ['%s'] using HTTP version [%s]\n\n", req.method, req.path, req.version);

        if (strstr(req.path, "html") != NULL || strcmp(req.path, "/") == 0) {
            head.value = "text/html";
        } else if (strstr(req.path, "css") != NULL) {
            head.value = "text/css";
        } else if (strstr(req.path, "js") != NULL) {
            head.value = "text/javascript";
        }

        if (strcmp(req.method, "GET") != 0) {
            status = "501 Not Implemented";

            head.value = "text/html";

            path = malloc(strlen(SERVER_DIR) + 10);

            strcpy(path, SERVER_DIR);
            strcpy(path + strlen(SERVER_DIR), "/501.html");
            path[strlen(SERVER_DIR) + 9] = 0;
        }

        if (strcmp(status, "200 OK") == 0) {
            if (strcmp(req.path, "/") == 0) {
                path = malloc(strlen(SERVER_DIR) + 12);

                strcpy(path, SERVER_DIR);
                strcpy(path + strlen(SERVER_DIR), "/index.html");
                path[strlen(SERVER_DIR) + 11] = 0;
            } else {
                path = malloc(strlen(SERVER_DIR) + strlen(req.path) + 1);
                
                strcpy(path, SERVER_DIR);
                strcpy(path + strlen(SERVER_DIR), req.path);
                path[strlen(SERVER_DIR) + strlen(req.path)] = 0;
            }
        }

        int ret;
        if ((ret = sendfile(cfd, &res, path)) < 0) {
            if (ret == -2) {
                free(path);

                head.value = "text/html";

                path = malloc(strlen(SERVER_DIR) + 10);

                strcpy(path, SERVER_DIR);
                strcpy(path + strlen(SERVER_DIR), "/404.html");
                path[strlen(SERVER_DIR) + 9] = 0;

                ret = sendfile(cfd, &res, path);
            }

            if (ret < 0) { // either we failed to send a 404 or failed to send a normal msg after getting the file
                fprintf(stderr, "Couldn't recover from error sending file (%i)\n", ret);

                free(path);
                freereq(&req);
                close(cfd);

                return -1;
            }
        }

        free(path);
        freereq(&req);
        close(cfd);

        return 0; // exit the child process
    }
}
