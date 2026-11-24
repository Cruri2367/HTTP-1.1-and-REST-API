#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200809L
#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <unistd.h>

#define BUFFER_SIZE 16384
#define ROOT_DIR "public"

static const char *reason_phrase(int code) {
    switch (code) {
        case 200: return "OK";
        case 201: return "Created";
        case 400: return "Bad Request";
        case 404: return "Not Found";
        case 405: return "Method Not Allowed";
        case 500: return "Internal Server Error";
        default: return "OK";
    }
}

static const char *content_type(const char *path) {
    const char *ext = strrchr(path, '.');
    if (!ext) return "application/octet-stream";
    if (strcmp(ext, ".html") == 0) return "text/html; charset=utf-8";
    if (strcmp(ext, ".css") == 0) return "text/css; charset=utf-8";
    if (strcmp(ext, ".js") == 0) return "application/javascript";
    if (strcmp(ext, ".json") == 0) return "application/json";
    if (strcmp(ext, ".txt") == 0) return "text/plain; charset=utf-8";
    return "application/octet-stream";
}

static void send_response(int fd, int code, const char *type,
                          const char *body, const char *extra_headers) {
    char header[2048];
    size_t body_len = strlen(body);
    int n = snprintf(header, sizeof(header),
        "HTTP/1.1 %d %s\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %zu\r\n"
        "Connection: close\r\n"
        "%s"
        "\r\n",
        code, reason_phrase(code), type, body_len,
        extra_headers ? extra_headers : "");
    if (n > 0) {
        (void)send(fd, header, (size_t)n, 0);
        (void)send(fd, body, body_len, 0);
    }
}

static int read_request(int fd, char *buf, size_t cap, size_t *header_end,
                        size_t *body_len) {
    size_t used = 0;
    *header_end = 0;
    *body_len = 0;

    while (used < cap - 1) {
        ssize_t n = recv(fd, buf + used, cap - 1 - used, 0);
        if (n <= 0) return -1;
        used += (size_t)n;
        buf[used] = '\0';

        char *sep = strstr(buf, "\r\n\r\n");
        if (sep) {
            *header_end = (size_t)(sep - buf) + 4;
            size_t expected = 0;
            char *cl = strcasestr(buf, "\r\nContent-Length:");
            if (cl) {
                cl += strlen("\r\nContent-Length:");
                while (*cl == ' ' || *cl == '\t') cl++;
                expected = (size_t)strtoul(cl, NULL, 10);
            }
            while (used - *header_end < expected && used < cap - 1) {
                n = recv(fd, buf + used, cap - 1 - used, 0);
                if (n <= 0) return -1;
                used += (size_t)n;
                buf[used] = '\0';
            }
            if (used - *header_end < expected) return -1;
            *body_len = expected;
            return 0;
        }
    }
    return -1;
}

static void serve_static(int fd, const char *path) {
    char full_path[1024];
    if (strstr(path, "..") || path[0] != '/') {
        send_response(fd, 400, "text/plain; charset=utf-8", "Bad path\n", NULL);
        return;
    }
    if (strcmp(path, "/") == 0) path = "/index.html";
    if (snprintf(full_path, sizeof(full_path), "%s%s", ROOT_DIR, path)
        >= (int)sizeof(full_path)) {
        send_response(fd, 404, "text/plain; charset=utf-8", "Not found\n", NULL);
        return;
    }

    FILE *f = fopen(full_path, "rb");
    if (!f) {
        send_response(fd, 404, "text/plain; charset=utf-8", "Not found\n", NULL);
        return;
    }

    char body[BUFFER_SIZE];
    size_t n = fread(body, 1, sizeof(body) - 1, f);
    body[n] = '\0';
    fclose(f);
    send_response(fd, 200, content_type(full_path), body, NULL);
}

static void handle_client(int fd) {
    char req[BUFFER_SIZE];
    size_t header_end = 0, body_len = 0;
    if (read_request(fd, req, sizeof(req), &header_end, &body_len) != 0) {
        send_response(fd, 400, "text/plain; charset=utf-8", "Bad request\n", NULL);
        return;
    }

    char method[16] = {0}, path[512] = {0}, version[16] = {0};
    if (sscanf(req, "%15s %511s %15s", method, path, version) != 3 ||
        strncmp(version, "HTTP/1.", 7) != 0) {
        send_response(fd, 400, "text/plain; charset=utf-8", "Bad request\n", NULL);
        return;
    }

    if (strcmp(method, "GET") == 0) {
        if (strcmp(path, "/api/users") == 0) {
            const char *json = "[{\"id\":1,\"name\":\"Ada\"},{\"id\":2,\"name\":\"Linus\"}]";
            send_response(fd, 200, "application/json", json, NULL);
        } else {
            serve_static(fd, path);
        }
    } else if (strcmp(method, "POST") == 0) {
        const char *body = req + header_end;
        if (body_len == 0 || body[0] != '{' || body[body_len - 1] != '}') {
            send_response(fd, 400, "application/json",
                          "{\"error\":\"Expected a JSON object\"}\n", NULL);
        } else {
            send_response(fd, 201, "application/json",
                          "{\"id\":3,\"message\":\"User created\"}\n",
                          "Location: /api/users/3\r\n");
        }
    } else {
        send_response(fd, 405, "text/plain; charset=utf-8",
                      "Method not allowed\n", "Allow: GET, POST\r\n");
    }
}

int main(int argc, char **argv) {
    int port = 8080;
    if (argc > 1) port = atoi(argv[1]);
    if (port < 1 || port > 65535) {
        fprintf(stderr, "Invalid port\n");
        return 1;
    }

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) { perror("socket"); return 1; }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons((unsigned short)port);

    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind"); close(server_fd); return 1;
    }
    if (listen(server_fd, 10) < 0) {
        perror("listen"); close(server_fd); return 1;
    }

    printf("HTTP server listening on http://127.0.0.1:%d\n", port);
    printf("Press Ctrl+C to stop.\n");
    for (;;) {
        int client_fd = accept(server_fd, NULL, NULL);
        if (client_fd < 0) {
            if (errno == EINTR) continue;
            perror("accept"); break;
        }
        handle_client(client_fd);
        close(client_fd);
    }
    close(server_fd);
    return 0;
}
