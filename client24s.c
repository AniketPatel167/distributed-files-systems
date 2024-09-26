#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define PORT 9999
#define BUFFER_SIZE 1024
#define MAX_TOKENS 3
#define EOF_MARKER "<EOF>"

static int sock = 0;
static struct sockaddr_in serv_addr;
static char buffer[BUFFER_SIZE] = {0};
static char *command;

void usage() {
    printf("\n --- USAGE --- ");
    printf(" ufile [filename] [destination_path] : upload \n");
    printf(" dfile [filename] : download \n");
    printf(" rmfile [filename] : remove/delete \n");
    printf(" dtar [filetype] : tar file \n");
    printf(" display [pathname] : ls \n");
}

// Function to send and receive command
char* send_recv_cmd() {
    // Send command to server
    send(sock, buffer, strlen(buffer), 0);
    printf("%s request sent to server\n", command);

    // Receive response from server
    static char response[BUFFER_SIZE]; // Static to ensure it persists after the function exits
    memset(response, 0, BUFFER_SIZE);
    recv(sock, response, BUFFER_SIZE, 0);
    return response;
}

// Function to upload a file to the server
int upload_file(int sock, const char *filename, const char *destination_path) {
    int file_fd;
    char data_buffer[BUFFER_SIZE];
    ssize_t bytes_read;
    size_t total_bytes_sent = 0;

    // Open the file to be uploaded
    file_fd = open(filename, O_RDONLY);
    if (file_fd < 0) {
        perror("Failed to open file");
        return -1;
    }

    // Send the file content in chunks
    while ((bytes_read = read(file_fd, data_buffer, BUFFER_SIZE)) > 0) {
        ssize_t bytes_sent = 0;
        while (bytes_sent < bytes_read) {
            ssize_t result = send(sock, data_buffer + bytes_sent, bytes_read - bytes_sent, 0);
            if (result < 0) {
                perror("Failed to send file data");
                close(file_fd);
                return -1;
            }
            bytes_sent += result;
        }
        total_bytes_sent += bytes_read;
    }

    if (bytes_read < 0) {
        perror("Failed to read file");
        close(file_fd);
        return -1;
    }

    // Send EOF marker
    send(sock, EOF_MARKER, strlen(EOF_MARKER), 0);

    // Close the file descriptor
    close(file_fd);

    printf("File '%s' sent successfully to '%s'. Total bytes sent: %zu\n", filename, destination_path, total_bytes_sent);
    return 0;
}

int main() {
    // Create socket
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("\n Socket creation error \n");
        return -1;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    // Convert IPv4 and IPv6 addresses from text to binary form
    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
        printf("\nInvalid address/ Address not supported \n");
        return -1;
    }

    // Connect to server
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        printf("\nConnection Failed \n");
        return -1;
    }

    while (1) {
        int flag = 0;
        printf("Enter command: ");
        fgets(buffer, BUFFER_SIZE, stdin);
        buffer[strcspn(buffer, "\n")] = 0; // Remove newline

        if (strcmp(buffer, "exit") == 0) {
            break;
        }

        char buff[BUFFER_SIZE];
        // Copy buffer to buff
        strcpy(buff, buffer);

        // Check valid command
        char *tokens[MAX_TOKENS]; // Array to store token pointers
        int token_count = 0; // Counter for tokens

        // Use strtok to tokenize the buffer
        char *token = strtok(buff, " "); // Initial call to strtok

        while (token != NULL) {
            if (token_count < MAX_TOKENS) {
                tokens[token_count++] = token; // Store the token
            } else {
                flag++;
                printf("Error : Too many arguments\n");
                usage();
                break; // Stop
            }
            token = strtok(NULL, " "); // Get the next token
        }

        if ((flag != 0) || (token_count == 0))
            continue;

        // Command matching
        if (strcmp(tokens[0], "ufile") == 0 && token_count == 3) {
            command = "Upload";
            char *response = send_recv_cmd();

            if (strcmp(response, "Upload") == 0) {
                printf("Uploading...");
                if (upload_file(sock, tokens[1], tokens[2]) == 0) {
                    recv(sock, response, BUFFER_SIZE, 0);
                }
            }
            printf("Server response: %s\n", response);

        } else if (strcmp(tokens[0], "dfile") == 0 && token_count == 2) {
            command = "Download";
        } else if (strcmp(tokens[0], "rmfile") == 0 && token_count == 2) {
            command = "Remove file";
        } else if (strcmp(tokens[0], "dtar") == 0 && token_count == 2) {
            command = "Tar";
        } else if (strcmp(tokens[0], "display") == 0 && token_count == 2) {
            command = "Display all files";
        } else if (strcmp(tokens[0], "help") == 0 && token_count == 1) {
            usage();
        } else {
            flag++;
            printf("INVALID SYNTAX : Use 'help' to print usage \n");
            continue;
        }

        if (flag != 0)
            continue;


    }

    close(sock);
    return 0;
}