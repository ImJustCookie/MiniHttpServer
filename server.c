#include <arpa/inet.h>
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <pthread.h>
#include <regex.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

char IP[32];
char PORT[8];
char ROOT_DIR[128];

void print_launching_screen()
{
    printf("\n");
    printf("===============================================================\n");
    printf("           Server Launched!             \n");
    printf("===============================================================\n");
    printf(" __  __ _      _ _  _ _   _        ___                      \n");
    printf("|  \\/  (_)_ _ (_) || | |_| |_ _ __/ __| ___ _ ___ _____ _ _ \n");
    printf("| |\\/| | | ' \\| | __ |  _|  _| '_ \\__ \\/ -_) '_\\ V / -_) '_|\n");
    printf("|_|  |_|_|_||_|_|_||_|\\__|\\__| .__/___/\\___|_|  \\_/\\___|_|  \n");
    printf("                              |_|                             \n");
    printf("===============================================================\n");
    printf("IP: %s\n", IP);
    printf("PORT: %s\n", PORT);
    printf("ROOT_DIR: %s\n", ROOT_DIR);
    printf("===============================================================\n");
    printf("===============================================================\n");
    printf("Running on : http://%s:%s/\n", IP, PORT);
    printf("===============================================================\n");
}

int read_conf_file()
{
    char buf[80];
    FILE *file;

    if ((file = fopen("./etc/web_server.conf", "r")) == NULL)
    {
        perror("cannot open ./etc/web_server.conf\n");
        return -1;
    }

    while (fgets(buf, sizeof buf, file))
    {
        if (buf[strspn(buf, " ")] == '\n')
            continue;
        sscanf(buf, "ip=%31s", IP);
        sscanf(buf, "port=%7s", PORT);
        sscanf(buf, "root_dir=%127s", ROOT_DIR);
    }
    fclose(file);

    return 0;
}

char *get_file_content(char *filepath, size_t *out_size)
{
    char fullpath[512];
    snprintf(fullpath, sizeof(fullpath), "%s/%s", ROOT_DIR, filepath);

    FILE *file = fopen(fullpath, "rb");
    if (!file)
        return NULL;

    fseek(file, 0, SEEK_END);
    *out_size = ftell(file);
    rewind(file);

    char *body = malloc(*out_size);
    fread(body, 1, *out_size, file);
    fclose(file);
    return body;
}

const char *get_mime_type(const char *path)
{
    const char *ext = strrchr(path, '.');
    if (!ext)
        return "application/octet-stream";
    if (strcmp(ext, ".html") == 0 || strcmp(ext, ".htm") == 0)
        return "text/html; charset=utf-8";
    if (strcmp(ext, ".jpg") == 0 || strcmp(ext, ".jpeg") == 0)
        return "image/jpeg";
    if (strcmp(ext, ".png") == 0)
        return "image/png";
    if (strcmp(ext, ".gif") == 0)
        return "image/gif";
    return "application/octet-stream";
}

char *generate_html(char *filepath, size_t *out_size, const char **out_mime, const char **status_code)
{
    char *body = get_file_content(filepath, out_size);
    if (!body)
    {

        const char *notfound =
            "<!DOCTYPE html>\n"
            "<html>\n"
            "<head>\n"
            "    <meta charset=\"UTF-8\">\n"
            "    <title>404 Not Found</title>\n"
            "</head>\n"
            "<body>\n"
            "    <h1>404 Not Found</h1>\n"
            "</body>\n"
            "</html>\n";
        *status_code = "404 Not Found";
        *out_size = strlen(notfound);
        *out_mime = "text/html; charset=utf-8";
        return strdup(notfound); // strdup needed so caller can free response
    }
    *status_code = "200 OK";
    *out_mime = get_mime_type(filepath);
    return body;
}

void *manage_client(void *arg)
{
    int client_fd = *((int *)arg);
    free(arg);

    char request[2048];
    int bytes = recv(client_fd, request, sizeof(request) - 1, 0);
    if (bytes <= 0)
    {
        close(client_fd);
        return NULL;
    }
    request[bytes] = '\0';

    char method[8], raw_path[256];
    sscanf(request, "%7s %255s", method, raw_path);

    char *path = raw_path;
    if (path[0] == '/')
        path++;

    if (strlen(path) == 0)
        strcpy(path, "index.html"); // root request

    if (strcmp(method, "GET") == 0)
    {
        size_t size;
        const char *mime;
        const char *status_code;
        char *body = generate_html(path, &size, &mime, &status_code);

        char header[512];

        int header_len = snprintf(header, sizeof(header),
                                  "HTTP/1.0 %s\r\n"
                                  "Content-Length: %zu\r\n"
                                  "Content-Type: %s\r\n"
                                  "\r\n",
                                  status_code, size, mime);
        send(client_fd, header, header_len, 0);
        send(client_fd, body, size, 0);
        free(body);
    }

    close(client_fd);
    return NULL;
}

int main(void)
{
    int server_fd;
    struct sockaddr_in server_addr;

    if (read_conf_file() == -1)
    {
        exit(EXIT_FAILURE);
    }

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) // create the server socket
    {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)); // needed to prevent weird bind bug on reset of server

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(atoi(PORT));
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_fd, // bind the server socket with the port and address
             (struct sockaddr *)&server_addr,
             sizeof(server_addr)) == -1)
    {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 10) != 0) // listen on the given port for incoming connections
    {
        perror("listen failed");
        exit(EXIT_FAILURE);
    }
    print_launching_screen();

    while (1)
    {
        struct sockaddr_in client_addr;
        socklen_t client_addr_len = sizeof(client_addr);
        int *client_fd = malloc(sizeof(int));

        if ((*client_fd = accept(server_fd, // accept new client connection
                                 (struct sockaddr *)&client_addr,
                                 &client_addr_len)) == -1)
        {
            perror("accept failed");
            free(client_fd);
            continue;
        }

        // create a new thread for each new client connection
        pthread_t thread_id;
        if (pthread_create(&thread_id, NULL, manage_client, (void *)client_fd) != 0)
        {
            perror("pthread_create failed:\n");
            free(client_fd);
            continue;
        }
        pthread_detach(thread_id);
    }

    close(server_fd); // close the connection with the socket;

    return 0;
}
