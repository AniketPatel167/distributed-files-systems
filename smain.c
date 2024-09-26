#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define PORT 9999
#define BUFFER_SIZE 1024
#define MAX_TOKENS 3
#define EOF_MARKER "<EOF>"

// Function to get the file extension
const char* get_file_extension(const char* filename) {
    // Find the last occurrence of the dot
    const char* dot = strrchr(filename, '.');
    if (dot && dot != filename) {
        // Return the extension part (after the dot)
        return dot + 1;
    }
    // Return empty string if no dot found or dot is at the beginning
    return "";
}

// Function to create a directory if it does not exist
int create_directory(const char *path) {
    // Construct the command string
    char command[1024];
    snprintf(command, sizeof(command), "mkdir -p %s", path);

    // Execute the command
    int result = system(command);

    // Check if the command was successful
    if (result != 0) {
        perror("Failed to create directory");
        return -1;
    }

    return 0;
}

// Function to handle file upload
int upload_file(int client_sock, char *tokens[]) {
    char *filename = tokens[1];
    char *destination_path = tokens[2];
    char full_path[1024];
    char data_buffer[BUFFER_SIZE];
    ssize_t bytes_received;
    int file_fd;

    // Construct the full path for the destination file
    snprintf(full_path, sizeof(full_path), "%s/%s", destination_path, filename);

    // Create the destination directory if it does not exist
    if (create_directory(destination_path) != 0) {
        send(client_sock, "Error while creating directory", 30, 0);
        return -1;
    }

    // Open the file for writing
    file_fd = open(full_path, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (file_fd < 0) {
        perror("Failed to open file");
        send(client_sock, "Error opening file", 19, 0);
        return -1;
    }
    // Send signal to upload
    send(client_sock, "Upload", 6, 0);

    // Receive the file data from the client
    while (1) {
        bytes_received = recv(client_sock, data_buffer, BUFFER_SIZE, 0);
        if (bytes_received <= 0) {
            perror("Failed to receive data");
            close(file_fd);
            send(client_sock, "Failed to receive data", 22, 0);
            return -1;
        }

        // Check for EOF marker
        if (bytes_received >= strlen(EOF_MARKER) &&
            memcmp(data_buffer + bytes_received - strlen(EOF_MARKER), EOF_MARKER, strlen(EOF_MARKER)) == 0) {
            // Write the data excluding the EOF marker
            if (write(file_fd, data_buffer, bytes_received - strlen(EOF_MARKER)) != bytes_received - strlen(EOF_MARKER)) {
                perror("Failed to write data to file");
                close(file_fd);
                send(client_sock, "Error writing data to file", 26, 0);
                return -1;
            }
            break; // Exit the loop as we've received the entire file
        } else {
            // Write the entire received data
            if (write(file_fd, data_buffer, bytes_received) != bytes_received) {
                perror("Failed to write data to file");
                close(file_fd);
                send(client_sock, "Error writing data to file", 26, 0);
                return -1;
            }
        }
    }

    // Close the file descriptor
    close(file_fd);
    printf("File '%s' uploaded successfully to '%s'.\n", filename, destination_path);
    send(client_sock, "File uploaded successfully", 27, 0);
    return 0;
}

void prcclient(int client_socket) {
    char buffer[BUFFER_SIZE];
    while (1) {
        int flag = 0;
        // Receive command from client
        ssize_t bytes_received = recv(client_socket, buffer, BUFFER_SIZE, 0);
        if (bytes_received <= 0) {
            continue;
        }
        buffer[bytes_received] = '\0';

        // Process command
        char *tokens[MAX_TOKENS]; // Array to store token pointers
        int token_count = 0;
        // Use strtok to tokenize the buffer
        char *token = strtok(buffer, " "); // Initial call to strtok

        while (token != NULL && token_count < MAX_TOKENS) {
            tokens[token_count++] = token; // Store the token
            token = strtok(NULL, " "); // Get the next token
        }

        if ((token_count == 0)) {
            send(client_socket, "Error while processing Command : token count = 0", 48, 0);
            continue;
        }

        // Command matching
        if (strcmp(tokens[0], "ufile") == 0 && token_count == 3) {
            upload_file(client_socket, tokens);
            continue;
        } else if (strcmp(tokens[0], "dfile") == 0 && token_count == 2) {
            //command = "Download";
        } else if (strcmp(tokens[0], "rmfile") == 0 && token_count == 2) {
            //command = "Remove file";
        } else if (strcmp(tokens[0], "dtar") == 0 && token_count == 2) {
            //command = "Tar";
        } else if (strcmp(tokens[0], "display") == 0 && token_count == 2) {
            //command = "Display all files";
        } else {
            send(client_socket, "Error while processing Command : Invalid Syntax", 47, 0);
            continue;
        }


        // Send response back to client
        // send(client_socket, "Command processed", 17, 0);
    }
    close(client_socket);
}

int main() {
    int server_fd, client_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);

    // Create socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    // Bind socket
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    // Listen for connections
    if (listen(server_fd, 3) < 0) {
        perror("listen failed");
        exit(EXIT_FAILURE);
    }

    printf("Smain server listening on port %d\n", PORT);

    while (1) {
        if ((client_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
            perror("accept failed");
            continue;
        }

        // Fork a child process to handle the client
        pid_t pid = fork();
        if (pid == 0) {
            // Child process
            close(server_fd);
            prcclient(client_socket);
            exit(0);
        } else if (pid > 0) {
            // Parent process
            close(client_socket);
        } else {
            perror("fork failed");
        }
    }

    return 0;
}