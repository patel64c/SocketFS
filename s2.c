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
            fprintf(stderr, "S2: Path too long\n");
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
void prcclient(int client) {
    char buffer[BUFFER_SIZE];
    char filepath[BUFFER_SIZE];
    ssize_t bytes_read;

    while (1) {
        // Clear buffer for new command
        memset(buffer, 0, BUFFER_SIZE);
        // Read command from client one byte at a time
        int buf_pos = 0;
        while (buf_pos < BUFFER_SIZE - 1) {
            bytes_read = read(client, buffer + buf_pos, 1); //read one byte 
            if (bytes_read <= 0) {
                perror("S2: read command");
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
            write(client, "No more commands\n", 17);
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
                    bytes_read = read(client, buffer + buf_pos, 1);
                    if (bytes_read <= 0) {
                        perror("S2: read filepath");
                        write(client, "ERROR: Read error\n", 18);
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
                    write(client, "No more files\n", 14);
                    break;
                }
                strncpy(filepath, buffer, sizeof(filepath) - 1);
                filepath[sizeof(filepath) - 1] = '\0';

                // Validate if file has .pdf extension
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
                    if (strcmp(ext_lower, ".pdf") == 0) {
                        valid = 1;
                    }
                }

                // Check if file type is valid
                if (!valid) {
                    printf("S2: Invalid file type: %s\n", filepath);
                    write(client, "ERROR: Invalid file type\n", 24);
                    continue;
                }

                // Delete the .pdf file
                if (unlink(filepath) == -1) {
                    perror("S2: unlink");
                    write(client, "ERROR: Failed to remove file\n", 28);
                } else {
                    printf("S2: Removed file %s\n", filepath);
                    write(client, "File removed\n", 13);
                }
            }
        // Process file upload command
        } else if (strcmp(command, "uploadf") == 0) {
            // Read destination path
            memset(buffer, 0, BUFFER_SIZE);
            buf_pos = 0;
            while (buf_pos < BUFFER_SIZE - 1) {
                bytes_read = read(client, buffer + buf_pos, 1);
                if (bytes_read <= 0) {
                    perror("S2: read destination");
                    write(client, "ERROR: Read error\n", 18);
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
                write(client, "No more paths\n", 14);
                return;
            }
            char destination_path[256];
            strncpy(destination_path, buffer, sizeof(destination_path) - 1);
            destination_path[sizeof(destination_path) - 1] = '\0';

            // Create directory path for file
            create_full_path(destination_path);

            while (1) {
                // Read filename from client
                memset(buffer, 0, BUFFER_SIZE);
                buf_pos = 0;
                while (buf_pos < BUFFER_SIZE - 1) {
                    bytes_read = read(client, buffer + buf_pos, 1);
                    if (bytes_read <= 0) {
                        perror("S2: read filename");
                        write(client, "ERROR: Read error\n", 18);
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
                    write(client, "No more files\n", 14);
                    break;
                }
                char filename[256];
                strncpy(filename, buffer, sizeof(filename) - 1);
                filename[sizeof(filename) - 1] = '\0';

                // Validate .pdf extension
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
                    if (strcmp(ext_lower, ".pdf") == 0) {
                        valid = 1;
                    }
                }

                if (!valid) {
                    printf("S2: Invalid file type: %s\n", filename);
                    write(client, "ERROR: Invalid file type\n", 24);
                    continue;
                }

                // Create full file path
                char full_file_path[BUFFER_SIZE];
                snprintf(full_file_path, sizeof(full_file_path), "%s/%s", destination_path, filename);

                // Open file for writing
                int file_fd = open(full_file_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if (file_fd < 0) {
                    perror("S2: open");
                    write(client, "ERROR: Failed to create file\n", 28);
                    continue;
                }

                // Receive file content until end marker
                int done_received = 0;
                char end_buffer[END_MARKER_LEN] = {0};
                int end_pos = 0;
                while (!done_received) {
                    bytes_read = read(client, buffer, 1);
                    if (bytes_read <= 0) {
                        close(file_fd);
                        write(client, "ERROR: Read error\n", 18);
                        return;
                    }
                    end_buffer[end_pos] = buffer[0];
                    end_pos++;
                    // Check for end marker
                    if (end_pos == END_MARKER_LEN) {
                        if (strncmp(end_buffer, END_MARKER, END_MARKER_LEN) == 0) {
                            done_received = 1;
                            break;
                        }
                        memmove(end_buffer, end_buffer + 1, END_MARKER_LEN - 1);
                        end_pos--;
                        write(file_fd, buffer, 1);
                    } else {
                        write(file_fd, buffer, 1);
                    }
                }

                // Close file and confirm upload
                close(file_fd);
                write(client, "File uploaded\n", 14);
            }
        // Process file download command
        } else if (strcmp(command, "downlf") == 0) {
            while (1) {
                // Read filepath from client
                memset(buffer, 0, BUFFER_SIZE);
                buf_pos = 0;
                while (buf_pos < BUFFER_SIZE - 1) {
                    bytes_read = read(client, buffer + buf_pos, 1);
                    if (bytes_read <= 0) {
                        perror("S2: read filepath");
                        write(client, "ERROR: Read error\n", 18);
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

                // Validate .pdf extension
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
                    if (strcmp(ext_lower, ".pdf") == 0) {
                        valid = 1;
                    }
                }

                if (!valid) {
                    printf("S2: Invalid file type: %s\n", filepath);
                    write(client, "ERROR: Invalid file type\n", 24);
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
                    perror("S2: open");
                    write(client, "ERROR: File not found\n", 22);
                    continue;
                }

                // Send filename to client
                write(client, filename, strlen(filename));
                write(client, "\n", 1);

                // Send file content
                while ((bytes_read = read(file_fd, buffer, BUFFER_SIZE)) > 0) {
                    write(client, buffer, bytes_read);
                }
                if (bytes_read < 0) {
                    perror("S2: read file");
                    write(client, "ERROR: Read error\n", 18);
                    close(file_fd);
                    continue;
                }
                // Send end marker
                write(client, END_MARKER, END_MARKER_LEN);
                close(file_fd);
                printf("S2: Sent %s to client\n", filepath);
            }
        // Handle unknown commands
        } else {
            printf("S2: Unknown command: %s\n", command);
            write(client, "ERROR: Unknown command\n", 22);
            // Skip to next command
            while (1) {
                bytes_read = read(client, buffer, 1);
                if (bytes_read <= 0 || buffer[0] == '\n') break;
            }
        }
    }
}

// Main server setup and client handling
int main(int argc, char *argv[]) {
    int sd, client, portNumber;
    struct sockaddr_in servAdd;

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
    if ((sd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        fprintf(stderr, "Could not create the socket\n");
        exit(2);
    }

    // Set up server address
    servAdd.sin_family = AF_INET;
    servAdd.sin_addr.s_addr = htonl(INADDR_ANY);
    servAdd.sin_port = htons((uint16_t)portNumber);

    // Bind socket to port
    if (bind(sd, (struct sockaddr *)&servAdd, sizeof(servAdd)) < 0) {
        fprintf(stderr, "Bind failed\n");
        close(sd);
        exit(3);
    }

    // Listen for incoming connections
    if (listen(sd, 5) < 0) {
        fprintf(stderr, "Listen failed\n");
        close(sd);
        exit(4);
    }

    // Accept and handle client connections
    while (1) {
        client = accept(sd, (struct sockaddr*)NULL, NULL);
        if (client < 0) {
            fprintf(stderr, "Client connection failed\n");
            continue;
        }

        // Fork a new process for each client
        int pid = fork();
        if (pid == 0) {
            close(sd); // Child closes server socket
            prcclient(client); // Handle client
            close(client); // Close client socket
            exit(0);
        } else if (pid > 0) {
            close(client); // Parent closes client socket
            waitpid(-1, NULL, WNOHANG); // Clean up zombie processes
        } else {
            perror("fork");
        }
    }

    // Close server socket
    close(sd);
    return 0;
}