#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <errno.h>
#include <ctype.h>
#include <arpa/inet.h>

#define BUFFER_SIZE 4096
#define END_MARKER "END_OF_FILE"
#define END_MARKER_LEN 11
#define MAX_RESPONSE_LEN 50

// Create a directory if it doesn't exist
void create_dir(const char *path) {
    if (mkdir(path, 0755) == -1 && errno != EEXIST) {
        perror("mkdir");
    }
}

// Build full directory path step by step
void create_full_path(const char *path) {
    char temp[BUFFER_SIZE];
    strncpy(temp, path, sizeof(temp) - 1);
    temp[sizeof(temp) - 1] = '\0';
    char *p = strtok(temp, "/");
    char full_path[BUFFER_SIZE] = "";
    while (p != NULL) {
        // Check for buffer overflow
        if (strlen(full_path) + strlen(p) + 2 >= BUFFER_SIZE) {
            fprintf(stderr, "S3: Path too long\n");
            return;
        }
        // Append path component
        if (strlen(full_path) > 0) {
            strcat(full_path, "/");
        }
        strcat(full_path, p);
        create_dir(full_path); // Create each subdirectory
        p = strtok(NULL, "/");
    }
}

// Handle client commands for file operations
void handle_client(int client_sock) {
    char buffer[BUFFER_SIZE];
    char dest_path[256];
    char filename[256];
    char filepath[BUFFER_SIZE];
    ssize_t bytes_read;

    while (1) {
        // Clear buffer for new command
        memset(buffer, 0, BUFFER_SIZE);
        // Read command from client
        int buf_pos = 0;
        while (buf_pos < BUFFER_SIZE - 1) {
            bytes_read = read(client_sock, buffer + buf_pos, 1);
            if (bytes_read <= 0) {
                perror("S3: read command");
                return;
            }
            if (buffer[buf_pos] == '\n') {
                buffer[buf_pos] = '\0';
                break;
            }
            buf_pos++;
        }
        // Check if no command was received
        if (buf_pos == 0) {
            write(client_sock, "No more commands\n", 17);
            return;
        }

        char command[256];
        strncpy(command, buffer, sizeof(command) - 1);
        command[sizeof(command) - 1] = '\0';

        // Process file removal command
        if (strcmp(command, "removef") == 0) {
            while (1) {
                // Read filepath from client
                memset(buffer, 0, BUFFER_SIZE);
                buf_pos = 0;
                while (buf_pos < BUFFER_SIZE - 1) {
                    bytes_read = read(client_sock, buffer + buf_pos, 1);
                    if (bytes_read <= 0) {
                        perror("S3: read filepath");
                        write(client_sock, "ERROR: Read error\n", 18);
                        return;
                    }
                    if (buffer[buf_pos] == '\n') {
                        buffer[buf_pos] = '\0';
                        break;
                    }
                    buf_pos++;
                }
                // Check if no filepath was received
                if (buf_pos == 0) {
                    write(client_sock, "No more files\n", 14);
                    break;
                }
                strncpy(filepath, buffer, sizeof(filepath) - 1);
                filepath[sizeof(filepath) - 1] = '\0';

                // Check if file has .txt extension
                char *ext = strrchr(filepath, '.');
                int valid = 0;
                if (ext && strlen(filepath) > strlen(ext)) {
                    char ext_lower[16];
                    strncpy(ext_lower, ext, sizeof(ext_lower) - 1);
                    ext_lower[sizeof(ext_lower) - 1] = '\0';
                    // Convert extension to lowercase
                    for (int j = 0; ext_lower[j]; j++) {
                        ext_lower[j] = tolower(ext_lower[j]);
                    }
                    if (strcmp(ext_lower, ".txt") == 0) {
                        valid = 1;
                    }
                }

                // Validate file type
                if (!valid) {
                    printf("S3: Invalid file type: %s\n", filepath);
                    write(client_sock, "ERROR: Invalid file type\n", 24);
                    continue;
                }

                // Delete the .txt file
                if (unlink(filepath) == -1) {
                    perror("S3: unlink");
                    write(client_sock, "ERROR: Failed to remove file\n", 28);
                } else {
                    printf("S3: Removed file %s\n", filepath);
                    write(client_sock, "File removed\n", 13);
                }
            }
        // Process file upload command
        } else if (strcmp(command, "uploadf") == 0) {
            // Read destination path
            memset(buffer, 0, BUFFER_SIZE);
            buf_pos = 0;
            while (buf_pos < BUFFER_SIZE - 1) {
                bytes_read = read(client_sock, buffer + buf_pos, 1);
                if (bytes_read <= 0) {
                    perror("S3: read destination");
                    write(client_sock, "ERROR: Read error\n", 18);
                    return;
                }
                if (buffer[buf_pos] == '\n') {
                    buffer[buf_pos] = '\0';
                    break;
                }
                buf_pos++;
            }
            // Check if no path was received
            if (buf_pos == 0) {
                write(client_sock, "No more paths\n", 14);
                return;
            }
            strncpy(dest_path, buffer, sizeof(dest_path) - 1);
            dest_path[sizeof(dest_path) - 1] = '\0';

            // Create directory path for file
            create_full_path(dest_path);

            while (1) {
                // Read filename from client
                memset(buffer, 0, BUFFER_SIZE);
                buf_pos = 0;
                while (buf_pos < BUFFER_SIZE - 1) {
                    bytes_read = read(client_sock, buffer + buf_pos, 1);
                    if (bytes_read <= 0) {
                        perror("S3: read filename");
                        write(client_sock, "ERROR: Read error\n", 18);
                        return;
                    }
                    if (buffer[buf_pos] == '\n') {
                        buffer[buf_pos] = '\0';
                        break;
                    }
                    buf_pos++;
                }
                // Check if no filename was received
                if (buf_pos == 0) {
                    write(client_sock, "No more files\n", 14);
                    break;
                }
                strncpy(filename, buffer, sizeof(filename) - 1);
                filename[sizeof(filename) - 1] = '\0';

                // Check if file has .txt extension
                char *ext = strrchr(filename, '.');
                int valid = 0;
                if (ext && strlen(filename) > strlen(ext)) {
                    char ext_lower[16];
                    strncpy(ext_lower, ext, sizeof(ext_lower) - 1);
                    ext_lower[sizeof(ext_lower) - 1] = '\0';
                    // Convert extension to lowercase
                    for (int j = 0; ext_lower[j]; j++) {
                        ext_lower[j] = tolower(ext_lower[j]);
                    }
                    if (strcmp(ext_lower, ".txt") == 0) {
                        valid = 1;
                    }
                }

                // Validate file type
                if (!valid) {
                    printf("S3: Invalid file type: %s\n", filename);
                    write(client_sock, "ERROR: Invalid file type\n", 24);
                    // Skip file content until end marker
                    char end_buffer[END_MARKER_LEN] = {0};
                    int end_pos = 0;
                    while (1) {
                        bytes_read = read(client_sock, buffer, 1);
                        if (bytes_read <= 0) {
                            write(client_sock, "ERROR: Read error\n", 18);
                            return;
                        }
                        end_buffer[end_pos++] = buffer[0];
                        if (end_pos == END_MARKER_LEN) {
                            if (strncmp(end_buffer, END_MARKER, END_MARKER_LEN) == 0) {
                                break;
                            }
                            memmove(end_buffer, end_buffer + 1, END_MARKER_LEN - 1);
                            end_pos--;
                        }
                    }
                    continue;
                }

                // Build full filepath
                char full_path[BUFFER_SIZE];
                snprintf(full_path, sizeof(full_path), "%s/%s", dest_path, filename);

                // Open file for writing
                int file_fd = open(full_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if (file_fd < 0) {
                    perror("S3: open");
                    write(client_sock, "ERROR: Failed to create file\n", 28);
                    // Skip file content until end marker
                    char end_buffer[END_MARKER_LEN] = {0};
                    int end_pos = 0;
                    while (1) {
                        bytes_read = read(client_sock, buffer, 1);
                        if (bytes_read <= 0) {
                            write(client_sock, "ERROR: Read error\n", 18);
                            return;
                        }
                        end_buffer[end_pos++] = buffer[0];
                        if (end_pos == END_MARKER_LEN) {
                            if (strncmp(end_buffer, END_MARKER, END_MARKER_LEN) == 0) {
                                break;
                            }
                            memmove(end_buffer, end_buffer + 1, END_MARKER_LEN - 1);
                            end_pos--;
                        }
                    }
                    continue;
                }

                // Receive file content
                char end_buffer[END_MARKER_LEN] = {0};
                int end_pos = 0;
                while (1) {
                    bytes_read = read(client_sock, buffer, 1);
                    if (bytes_read <= 0) {
                        perror("S3: read file content");
                        close(file_fd);
                        write(client_sock, "ERROR: Read error\n", 18);
                        return;
                    }
                    end_buffer[end_pos++] = buffer[0];
                    // Check for end marker
                    if (end_pos == END_MARKER_LEN) {
                        if (strncmp(end_buffer, END_MARKER, END_MARKER_LEN) == 0) {
                            break;
                        }
                        memmove(end_buffer, end_buffer + 1, END_MARKER_LEN - 1);
                        end_pos--;
                        // Write byte to file
                        if (write(file_fd, buffer, 1) != 1) {
                            perror("S3: write to file");
                            close(file_fd);
                            write(client_sock, "ERROR: Write error\n", 19);
                            return;
                        }
                    } else {
                        // Write byte to file
                        if (write(file_fd, buffer, 1) != 1) {
                            perror("S3: write to file");
                            close(file_fd);
                            write(client_sock, "ERROR: Write error\n", 19);
                            return;
                        }
                    }
                }
                // Close file and confirm upload
                close(file_fd);
                write(client_sock, "File uploaded\n", 14);
                printf("S3: Saved file: %s\n", full_path);
            }
        // Process file download command
        } else if (strcmp(command, "downlf") == 0) {
            while (1) {
                // Read filepath from client
                memset(buffer, 0, BUFFER_SIZE);
                buf_pos = 0;
                while (buf_pos < BUFFER_SIZE - 1) {
                    bytes_read = read(client_sock, buffer + buf_pos, 1);
                    if (bytes_read <= 0) {
                        perror("S3: read filepath");
                        write(client_sock, "ERROR: Read error\n", 18);
                        return;
                    }
                    if (buffer[buf_pos] == '\n') {
                        buffer[buf_pos] = '\0';
                        break;
                    }
                    buf_pos++;
                }
                // Check if no filepath was received
                if (buf_pos == 0) {
                    break;
                }
                strncpy(filepath, buffer, sizeof(filepath) - 1);
                filepath[sizeof(filepath) - 1] = '\0';

                // Check if file has .txt extension
                char *ext = strrchr(filepath, '.');
                int valid = 0;
                if (ext && strlen(filepath) > strlen(ext)) {
                    char ext_lower[16];
                    strncpy(ext_lower, ext, sizeof(ext_lower) - 1);
                    ext_lower[sizeof(ext_lower) - 1] = '\0';
                    // Convert extension to lowercase
                    for (int j = 0; ext_lower[j]; j++) {
                        ext_lower[j] = tolower(ext_lower[j]);
                    }
                    if (strcmp(ext_lower, ".txt") == 0) {
                        valid = 1;
                    }
                }

                // Validate file type
                if (!valid) {
                    printf("S3: Invalid file type: %s\n", filepath);
                    write(client_sock, "ERROR: Invalid file type\n", 24);
                    continue;
                }

                // Extract filename from filepath
                char *filename = strrchr(filepath, '/');
                if (filename == NULL) {
                    filename = filepath;
                } else {
                    filename++;
                }

                // Open file for reading
                int file_fd = open(filepath, O_RDONLY);
                if (file_fd < 0) {
                    perror("S3: open");
                    write(client_sock, "ERROR: File not found\n", 22);
                    continue;
                }

                // Send filename to client
                write(client_sock, filename, strlen(filename));
                write(client_sock, "\n", 1);

                // Send file content
                while ((bytes_read = read(file_fd, buffer, BUFFER_SIZE)) > 0) {
                    write(client_sock, buffer, bytes_read);
                }
                if (bytes_read < 0) {
                    perror("S3: read file");
                    write(client_sock, "ERROR: Read error\n", 18);
                    close(file_fd);
                    continue;
                }
                // Send end marker
                write(client_sock, END_MARKER, END_MARKER_LEN);
                close(file_fd);
                printf("S3: Sent %s to client\n", filepath);
            }
        // Handle unknown commands
        } else {
            printf("S3: Unknown command: %s\n", command);
            write(client_sock, "ERROR: Unknown command\n", 22);
            // Skip to next command
            while (1) {
                bytes_read = read(client_sock, buffer, 1);
                if (bytes_read <= 0 || buffer[0] == '\n') break;
            }
        }
    }
}

// Main server setup and client handling
int main(int argc, char *argv[]) {
    int server_sock, client_sock, portNumber;
    struct sockaddr_in server_addr;
    socklen_t addr_len = sizeof(struct sockaddr_in);

    // Check for correct port argument
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <Port#>\n", argv[0]);
        exit(1);
    }

    // Validate port number
    if (sscanf(argv[1], "%d", &portNumber) != 1 || portNumber <= 0 || portNumber > 65535) {
        fprintf(stderr, "Invalid port number: %s\n", argv[1]);
        exit(1);
    }

    // Create TCP socket
    server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock < 0) {
        perror("socket");
        exit(2);
    }

    // Allow socket reuse
    int opt = 1;
    if (setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt");
        close(server_sock);
        exit(2);
    }

    // Set up server address
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons((uint16_t)portNumber);

    // Bind socket to port
    if (bind(server_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind");
        close(server_sock);
        exit(3);
    }

    // Listen for incoming connections
    if (listen(server_sock, 5) < 0) {
        perror("listen");
        close(server_sock);
        exit(4);
    }

    // Show server is ready
    printf("S3: Server listening on port %d...\n", portNumber);

    // Accept and handle client connections
    while (1) {
        client_sock = accept(server_sock, (struct sockaddr*)NULL, NULL);
        if (client_sock < 0) {
            perror("accept");
            continue;
        }

        // Fork a new process for each client
        int pid = fork();
        if (pid == 0) {
            close(server_sock); // Child closes server socket
            handle_client(client_sock); // Handle client
            close(client_sock); // Close client socket
            exit(0);
        } else if (pid > 0) {
            close(client_sock); // Parent closes client socket
            waitpid(-1, NULL, WNOHANG); // Clean up zombie processes
        } else {
            perror("fork");
        }
    }

    // Close server socket
    close(server_sock);
    return 0;
}