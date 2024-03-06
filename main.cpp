#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/reboot.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <fcntl.h>

#define PORT 8080
#define SERVER "Server: system-control-server-in-c\r\n"
#define DOCUMENT_ROOT "/opt/Reboot-Panel/"

void serve_file(int client_sock, const char* path) {
    char file_path[256];
    sprintf(file_path, "%s%s", DOCUMENT_ROOT, path);
    int file_fd = open(file_path, O_RDONLY);
    if (file_fd == -1) {
        // 文件不存在或无法打开，发送404响应
        char* response =
            "HTTP/1.1 404 Not Found\r\n"
            SERVER
            "Content-Type: text/plain; charset=UTF-8\r\n\r\n"
            "404 Not Found\r\n";
        write(client_sock, response, strlen(response));
    }
    else {
        // 发送200响应
        char* status_line = "HTTP/1.1 200 OK\r\n";
        write(client_sock, status_line, strlen(status_line));
        write(client_sock, SERVER, strlen(SERVER));
        char* content_type = "Content-Type: text/html; charset=UTF-8\r\n\r\n";
        write(client_sock, content_type, strlen(content_type));
        // 读取并发送文件内容
        char buffer[1024];
        ssize_t bytes_read;
        while ((bytes_read = read(file_fd, buffer, sizeof(buffer))) > 0) {
            write(client_sock, buffer, bytes_read);
        }
        close(file_fd);
    }
}

void handle_reboot(int client_sock) {
    // 发送确认响应
    char* response =
        "HTTP/1.1 200 OK\r\n"
        SERVER
        "Content-Type: text/plain; charset=UTF-8\r\n\r\n"
        "系统正在重启...\r\n";

    write(client_sock, response, strlen(response));

    // 执行重启操作
    sync(); // 同步数据到磁盘
    reboot(RB_AUTOBOOT); // 自动重启
}

void process_request(int client_sock) {
    char buffer[1024];
    int bytes_read = read(client_sock, buffer, sizeof(buffer) - 1);
    if (bytes_read > 0) {
        buffer[bytes_read] = '\0';

        // 检查是否为重启请求
        if (strstr(buffer, "POST /reboot") != NULL) {
            handle_reboot(client_sock);
        }
        else {
            // 其他请求，假设访问根路径
            serve_file(client_sock, "index.html");
        }
    }
}

int main() {
    int server_fd, client_sock;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    char path[256] = "/"; // 默认路径
    // 创建套接字
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        perror("Could not create socket");
        exit(EXIT_FAILURE);
    }
    // 配置服务器地址结构
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);
    // 绑定套接字到端口
    if (bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }
    // 监听端口
    if (listen(server_fd, 10) < 0) {
        perror("Listen failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }
    printf("HTTP server is running on port %d...\n", PORT);

    while (1) {
        client_sock = accept(server_fd, (struct sockaddr*)&client_addr, &client_addr_len);
        if (client_sock < 0) {
            perror("Accept failed");
            continue;
        }

        process_request(client_sock);

        // 关闭客户端套接字
        close(client_sock);
    }

    return 0;
}
