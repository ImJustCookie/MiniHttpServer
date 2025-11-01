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

char *get_file_content(char *filepath)
{
    char fullpath[512];
    snprintf(fullpath, sizeof(fullpath), "%s/%s", ROOT_DIR, filepath);

    FILE *file = fopen(fullpath, "r");
    if (!file)
    {
        return NULL;
    }

    fseek(file, 0, SEEK_END);
    size_t size = ftell(file);
    rewind(file);

    char *body = malloc(size + 1);
    fread(body, 1, size, file);
    body[size] = '\0';
    fclose(file);
    return body;
}

char *generate_html(char *filepath)
{
    char *body = get_file_content(filepath);
    char *response = NULL;
    if (!body)
    {
        // strdup needed so that the call function can free the response afterward
        response = strdup("HTTP/1.0 404 ERROR\r\n"
                          "Content-Length: 27\r\n"
                          "Content-Type: text/html; charset=utf-8\r\n"
                          "\r\n"
                          "<h1>404 FILE NOT FOUND</h1>");
    }
    else
    {
        size_t needed = snprintf(NULL, 0,
                                 "HTTP/1.0 200 OK\r\n"
                                 "Content-Length: %zu\r\n"
                                 "Content-Type: text/html; charset=utf-8\r\n"
                                 "\r\n"
                                 "%s",
                                 strlen(body), body);

        response = malloc(needed + 1);

        snprintf(response, needed + 1,
                 "HTTP/1.0 200 OK\r\n"
                 "Content-Length: %zu\r\n"
                 "Content-Type: text/html; charset=utf-8\r\n"
                 "\r\n"
                 "%s",
                 strlen(body), body);

        free(body);
    }
    return response;
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

    // Handle root request
    if (strlen(path) == 0)
        strcpy(path, "index.html");

    char *response = NULL;
    if (strcmp(method, "GET") == 0)
    {
        response = generate_html(path);
        send(client_fd, response, strlen(response), 0);
        free(response);
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
    printf("IP: %s, PORT: %s, ROOT_DIR: %s\n", IP, PORT, ROOT_DIR);

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
    printf("Server started on port: %s !\n", PORT);

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
