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

void create_dir(const char *path) {
    //make a folder with read/write/execute
    // If it already exists, fine 
    if (mkdir(path, 0755) == -1 && errno != EEXIST) {
        perror("mkdir"); // Print error
    }
}

void create_full_path(const char *path) {
    char temp[BUFFER_SIZE];
    strncpy(temp, path, sizeof(temp) - 1);
    temp[sizeof(temp) - 1] = '\0';  // Make sure string ends properly

    // Break the path into parts  separated by "/"
    char *p = strtok(temp, "/");
    char full_path[BUFFER_SIZE] = "";

    while (p != NULL) {
        // Check if adding this folder will make the path too long
        if (strlen(full_path) + strlen(p) + 2 >= BUFFER_SIZE) {
            fprintf(stderr, "S4: Path too long\n");
            return;
        }

        // Add "/" between folders after the first one
        if (strlen(full_path) > 0) {
            strcat(full_path, "/");
        }

        
        strcat(full_path, p);

        // Make the folder if it doesn't exist
        create_dir(full_path);

        // Move to the next folder in the path
        p = strtok(NULL, "/");
    }
}


void handle_client(int client_sock) {
    char buffer[BUFFER_SIZE];
    char dest_path[256];
    char filename[256];
    char filepath[BUFFER_SIZE];
    ssize_t bytes_read;

    while (1) {
        // Clear buffer
        memset(buffer, 0, BUFFER_SIZE);
        // Read command
        int buf_pos = 0;
        while (buf_pos < BUFFER_SIZE - 1) {
            bytes_read = read(client_sock, buffer + buf_pos, 1);
            if (bytes_read <= 0) {
                perror("S4: read command");
                return;
            }
            if (buffer[buf_pos] == '\n') {
                buffer[buf_pos] = '\0';
                break;
            }
            buf_pos++;
        }
        if (buf_pos == 0) {
            write(client_sock, "No more commands\n", 17);
            return;
        }

        char command[256];
        strncpy(command, buffer, sizeof(command) - 1);
        command[sizeof(command) - 1] = '\0';

        if (strcmp(command, "removef") == 0) {
            while (1) {
                // Read filepath
                memset(buffer, 0, BUFFER_SIZE);
                buf_pos = 0;
                while (buf_pos < BUFFER_SIZE - 1) {
                    bytes_read = read(client_sock, buffer + buf_pos, 1);
                    if (bytes_read <= 0) {
                        perror("S4: read filepath");
                        write(client_sock, "ERROR: Read error\n", 18);
                        return;
                    }
                    if (buffer[buf_pos] == '\n') {
                        buffer[buf_pos] = '\0';
                        break;
                    }
                    buf_pos++;
                }
                if (buf_pos == 0) {
                    write(client_sock, "No more files\n", 14);
                    break;
                }
                strncpy(filepath, buffer, sizeof(filepath) - 1);
                filepath[sizeof(filepath) - 1] = '\0';

                // Validate file extension
                char *ext = strrchr(filepath, '.');
                int valid = 0;
                if (ext && strlen(filepath) > strlen(ext)) {
                    char ext_lower[16];
                    strncpy(ext_lower, ext, sizeof(ext_lower) - 1);
                    ext_lower[sizeof(ext_lower) - 1] = '\0';
                    for (int j = 0; ext_lower[j]; j++) {
                        ext_lower[j] = tolower(ext_lower[j]);
                    }
                    if (strcmp(ext_lower, ".zip") == 0) {
                        valid = 1;
                    }
                }

                if (!valid) {
                    printf("S4: Invalid file type: %s\n", filepath);
                    write(client_sock, "ERROR: Invalid file type\n", 24);
                    continue;
                }

                // Delete .zip file locally
                if (unlink(filepath) == -1) {
                    perror("S4: unlink");
                    write(client_sock, "ERROR: Failed to remove file\n", 28);
                } else {
                    printf("S4: Removed file %s\n", filepath);
                    write(client_sock, "File removed\n", 13);
                }
            }
        } else if (strcmp(command, "uploadf") == 0) {
            // Read destination path
            memset(buffer, 0, BUFFER_SIZE);
            buf_pos = 0;
            while (buf_pos < BUFFER_SIZE - 1) {
                bytes_read = read(client_sock, buffer + buf_pos, 1);
                if (bytes_read <= 0) {
                    perror("S4: read destination");
                    write(client_sock, "ERROR: Read error\n", 18);
                    return;
                }
                if (buffer[buf_pos] == '\n') {
                    buffer[buf_pos] = '\0';
                    break;
                }
                buf_pos++;
            }
            if (buf_pos == 0) {
                write(client_sock, "No more paths\n", 14);
                return;
            }
            strncpy(dest_path, buffer, sizeof(dest_path) - 1);
            dest_path[sizeof(dest_path) - 1] = '\0';

            create_full_path(dest_path);

            while (1) {
                // Read filename
                memset(buffer, 0, BUFFER_SIZE);
                buf_pos = 0;
                while (buf_pos < BUFFER_SIZE - 1) {
                    bytes_read = read(client_sock, buffer + buf_pos, 1);
                    if (bytes_read <= 0) {
                        perror("S4: read filename");
                        write(client_sock, "ERROR: Read error\n", 18);
                        return;
                    }
                    if (buffer[buf_pos] == '\n') {
                        buffer[buf_pos] = '\0';
                        break;
                    }
                    buf_pos++;
                }
                if (buf_pos == 0) {
                    write(client_sock, "No more files\n", 14);
                    break;
                }
                strncpy(filename, buffer, sizeof(filename) - 1);
                filename[sizeof(filename) - 1] = '\0';

                // Validate file extension
                char *ext = strrchr(filename, '.');
                int valid = 0;
                if (ext && strlen(filename) > strlen(ext)) {
                    char ext_lower[16];
                    strncpy(ext_lower, ext, sizeof(ext_lower) - 1);
                    ext_lower[sizeof(ext_lower) - 1] = '\0';
                    for (int j = 0; ext_lower[j]; j++) {
                        ext_lower[j] = tolower(ext_lower[j]);
                    }
                    if (strcmp(ext_lower, ".zip") == 0) {
                        valid = 1;
                    }
                }

                if (!valid) {
                    printf("S4: Invalid file type: %s\n", filename);
                    write(client_sock, "ERROR: Invalid file type\n", 24);
                    // Skip content until END_MARKER
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

                int file_fd = open(full_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if (file_fd < 0) {
                    perror("S4: open");
                    write(client_sock, "ERROR: Failed to create file\n", 28);
                    // Skip content until END_MARKER
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
                        perror("S4: read file content");
                        close(file_fd);
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
                        if (write(file_fd, buffer, 1) != 1) {
                            perror("S4: write to file");
                            close(file_fd);
                            write(client_sock, "ERROR: Write error\n", 19);
                            return;
                        }
                    } else {
                        if (write(file_fd, buffer, 1) != 1) {
                            perror("S4: write to file");
                            close(file_fd);
                            write(client_sock, "ERROR: Write error\n", 19);
                            return;
                        }
                    }
                }
                close(file_fd);
                write(client_sock, "File uploaded\n", 14);
                printf("S4: Saved file: %s\n", full_path);
            }
        } else if (strcmp(command, "downlf") == 0) {
            while (1) {
                // Read filepath
                memset(buffer, 0, BUFFER_SIZE);
                buf_pos = 0;
                while (buf_pos < BUFFER_SIZE - 1) {
                    bytes_read = read(client_sock, buffer + buf_pos, 1);
                    if (bytes_read <= 0) {
                        perror("S4: read filepath");
                        write(client_sock, "ERROR: Read error\n", 18);
                        return;
                    }
                    if (buffer[buf_pos] == '\n') {
                        buffer[buf_pos] = '\0';
                        break;
                    }
                    buf_pos++;
                }
                if (buf_pos == 0) {
                    break;
                }
                strncpy(filepath, buffer, sizeof(filepath) - 1);
                filepath[sizeof(filepath) - 1] = '\0';

            // Check if the file has a valid extensio
char *ext = strrchr(filepath, '.');  // Find the last 
int valid = 0;  // Assume file is invalid

// Make sure there is an ext
if (ext && strlen(filepath) > strlen(ext)) {
    char ext_lower[16];
    strncpy(ext_lower, ext, sizeof(ext_lower) - 1);
    ext_lower[sizeof(ext_lower) - 1] = '\0';  // Null termin string

    // Convert the extension to lowercase for comparison
    for (int j = 0; ext_lower[j]; j++) {
        ext_lower[j] = tolower(ext_lower[j]);
    }

    // Check if the extension is exactly 
    if (strcmp(ext_lower, ".zip") == 0) {
        valid = 1; 
    }
}


                if (!valid) {
                    printf("S4: Invalid file type: %s\n", filepath);
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

                // Handle .zip file locally
                int file_fd = open(filepath, O_RDONLY);
                if (file_fd < 0) {
                    perror("S4: open");
                    write(client_sock, "ERROR: File not found\n", 22);
                    continue;
                }

                // Send filename
                write(client_sock, filename, strlen(filename));
                write(client_sock, "\n", 1);

                // Send file content
                while ((bytes_read = read(file_fd, buffer, BUFFER_SIZE)) > 0) {
                    write(client_sock, buffer, bytes_read);
                }
                if (bytes_read < 0) {
                    perror("S4: read file");
                    write(client_sock, "ERROR: Read error\n", 18);
                    close(file_fd);
                    continue;
                }
                write(client_sock, END_MARKER, END_MARKER_LEN);
                close(file_fd);
                printf("S4: Sent %s to client\n", filepath);
            }
        } else {
            printf("S4: Unknown command: %s\n", command);
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
    printf("S4: Server listening on port %d...\n", portNumber);

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