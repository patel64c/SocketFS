#include <netinet/in.h>  // For internet address structures
#include <stdio.h>     // For standard input/output functions
#include <stdlib.h>       
#include <sys/socket.h>  // For socket functions
#include <sys/types.h>   // For data types used in system calls
#include <string.h>      
#include <unistd.h>    
#include <fcntl.h>       
#include <sys/wait.h>  // For wait functions
#include <sys/stat.h>      // For file status functions
#include <errno.h>       
#include <ctype.h>         // For character type functions
#include <arpa/inet.h>// For internet operations

#define BUFFER_SIZE 4096   // Size of buffer for reading/writing data
#define END_MARKER "END_OF_FILE" // Marker to signal end of file
#define END_MARKER_LEN 11  // Length of end marker string
#define MAX_RESPONSE_LEN 50 // Max bytes to read for server response

int port_s2 = 0; // pdf
char ip_add_s2[256] = "127.0.0.1";

int port_s3 = 0; // txt
char ip_add_s3[256] = "127.0.0.1";

int port_s4 = 0; // zip
char ip_add_s4[256] = "127.0.0.1";

// Function to create a directory
void create_dir(const char *path) {
    // Try to make directory with permissions 0755
    if (mkdir(path, 0755) == -1 && errno != EEXIST) { // If creation fails and directory doesn't already exist
        perror("mkdir");
    }
}

// Function to create all directories in a path
void create_full_path(const char *path) {
    char temp[BUFFER_SIZE]; // Buffer to hold copy of path
    strncpy(temp, path, sizeof(temp) - 1); 
    temp[sizeof(temp) - 1] = '\0'; // Ensure string ends
    char *p = strtok(temp, "/"); // Split path by '/'
    char full_path[BUFFER_SIZE] = ""; 
    while (p != NULL) { 
        if (strlen(full_path) > 0) { 
            strcat(full_path, "/"); // Add separator
        }
        strcat(full_path, p); // Add current part
        create_dir(full_path); // Create directory for current path
        p = strtok(NULL, "/");
    }
}

// Function to forward a file to another server
int forward_file_to_server(const char *filename, const char *dest_path, int port, char *forward_ip) {
    // Create a TCP socket
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) { // If socket creation fails
        perror("forward: socket");
        return -1; // Return error code
    }

    // Set up server address details
    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port); // Convert port number to network format
    serv_addr.sin_addr.s_addr = inet_addr(forward_ip); // Convert IP string to binary

    // Try to connect to the server
    if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("forward: connect"); // Show error if connection fails
        close(sockfd); // Close the socket
        return -1; // Return error code
    }
   
    write(sockfd, "uploadf", 7); // Send "uploadf" command
    write(sockfd, "\n", 1);      
    // Send where to save the file
    write(sockfd, dest_path, strlen(dest_path)); // Send destination path
    write(sockfd, "\n", 1);   // Add newline
 
    write(sockfd, filename, strlen(filename)); // Send filename
    write(sockfd, "\n", 1); // Add newline

    char full_path[BUFFER_SIZE];
    snprintf(full_path, sizeof(full_path), "%s/%s", dest_path, filename);
    int fd = open(full_path, O_RDONLY);
    if (fd < 0) {
        perror("open file");
        close(sockfd);
        return -1;
    }

       // Buffer for reading file data
    char buffer[BUFFER_SIZE];
    ssize_t n;
    // Read file data in chunks
    while ((n = read(fd, buffer, BUFFER_SIZE)) > 0) {
        ssize_t total_written = 0; 
        // Ensure all read data is sent
        while (total_written < n) {
            // Write data to socket
            ssize_t written = write(sockfd, buffer + total_written, n - total_written);
            if (written < 0) { 
                perror("forward: write file data"); 
                close(fd); // Close file
                close(sockfd); // Close socket
                return -1; 
            }
            total_written += written; // Update total written
        }
    }
    if (n < 0) { // If reading file fails
        perror("forward: read file");
        close(fd); // Close file
        close(sockfd); // Close socket
        return -1; 
    }
    close(fd);
    write(sockfd, END_MARKER, END_MARKER_LEN);

    char resp[MAX_RESPONSE_LEN]; // String to store response
int pos = 0;
while (pos < MAX_RESPONSE_LEN - 1) {
    n = read(sockfd, resp + pos, 1); // Read one character
    if (n <= 0) break;
    if (resp[pos] == '\n') {
        resp[pos] = '\0'; // End string at newline
        break;
    }
    pos++;
}
    resp[pos] = '\0';
    close(sockfd);

    if (strcmp(resp, "File uploaded") == 0) { //compare string 
        return 0;
    } else {
        fprintf(stderr, "forward: server response: %s\n", resp);
        return -1;
    }
}

int forward_remove_to_server(const char *filepath, int port, char *forward_ip) {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0); // Create a socket for connection
    if (sockfd < 0) { // Check if socket creation failed
        perror("forward: socket"); 
        return -1; // Return error code
    }

   struct sockaddr_in serv_addr; // Structure to hold server address info
serv_addr.sin_family = AF_INET; 
serv_addr.sin_port = htons(port); // Convert port number to network format
serv_addr.sin_addr.s_addr = inet_addr(forward_ip); // Convert IP address string to binary

if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) { 
    perror("forward: connect"); // Print error if connection fails
    close(sockfd); // Close socket
    return -1; 
}

write(sockfd, "removef", 7); // Send "removef" command
write(sockfd, "\n", 1); 
write(sockfd, filepath, strlen(filepath)); // Send file path
write(sockfd, "\n", 1); // Send  newline

 char resp[MAX_RESPONSE_LEN]; // Buffer for server response
int pos = 0;
while (pos < MAX_RESPONSE_LEN - 1) {
    ssize_t n = read(sockfd, resp + pos, 1); // Read one character
    if (n <= 0) break;
    if (resp[pos] == '\n') {
        resp[pos] = '\0'; // End string at newline
        break;
    }
    pos++;
}
resp[pos] = '\0'; // Ensure string is terminated
close(sockfd); // Close socket

if (strcmp(resp, "File removed") == 0) { // Check if  was removed
    return 0; // Success
} else {
    fprintf(stderr, "forward: server response: %s\n", resp); // Print error response
    return -1; 
}
}

int forward_download_from_server(const char *filepath, int client, int port, char *forward_ip) {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0); // Create socket for connection
    if (sockfd < 0) { // Check if socket creation fails
        perror("forward: socket"); 
        return -1; // Return error code
    }

struct sockaddr_in serv_addr; // Structure for server address
serv_addr.sin_family = AF_INET; // Set to IPv4 protocol
serv_addr.sin_port = htons(port); // Convert port to network format
serv_addr.sin_addr.s_addr = inet_addr(forward_ip); // Convert IP string to binary

if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) { // Try to connect to server
    perror("forward: connect"); // Print error if connection fails
    close(sockfd); // Close socket
    return -1; // Return error code
}

write(sockfd, "downlf", 6); // Send "downlf" command for download
write(sockfd, "\n", 1); // Send newline
write(sockfd, filepath, strlen(filepath)); // Send file path
write(sockfd, "\n", 1);
write(sockfd, "\n", 1); 

    char buffer[BUFFER_SIZE];
    ssize_t bytes_read;

    // Read filename response
    int buf_pos = 0;
    while (buf_pos < MAX_RESPONSE_LEN - 1) {
        bytes_read = read(sockfd, buffer + buf_pos, 1);
        if (bytes_read <= 0) {
            close(sockfd);
            return -1;
        }
        if (buffer[buf_pos] == '\n') {
            buffer[buf_pos] = '\0';
            break;
        }
        buf_pos++;
    }

    if (strncmp(buffer, "ERROR", 5) == 0) { //checks for an error
        write(client, buffer, strlen(buffer));
        write(client, "\n", 1);
        close(sockfd);
        return -1;
    }

    // Forward filename to client
    write(client, buffer, strlen(buffer));
    write(client, "\n", 1);

    // Receive and forward file content
    while (1) {
        bytes_read = read(sockfd, buffer, BUFFER_SIZE);
        if (bytes_read <= 0) {
            close(sockfd);
            return -1;
        }

        if (bytes_read >= END_MARKER_LEN && 
    strncmp(buffer + bytes_read - END_MARKER_LEN, END_MARKER, END_MARKER_LEN) == 0) { // Check if data ends with end marker
    write(client, buffer, bytes_read); // Send final chunk to client
    break; 
}
        write(client, buffer, bytes_read);
    }

    close(sockfd);
    return 0;
}

void prcclient(int client) {
    char buffer[BUFFER_SIZE]; //buffer soterage  
    char filepath[BUFFER_SIZE];
    ssize_t bytes_read;

    while (1) {
        memset(buffer, 0, BUFFER_SIZE);
        int buf_pos = 0;
        while (buf_pos < BUFFER_SIZE - 1) {
            bytes_read = read(client, buffer + buf_pos, 1); //read one char 
            if (bytes_read <= 0) {
                perror("S1: read command");
                return;
            }
            if (buffer[buf_pos] == '\n') { //newl ine  
                buffer[buf_pos] = '\0';
                break;
            }
            buf_pos++; //next char 
        }
        if (buf_pos == 0) {
            write(client, "No more commands\n", 17);
            return;
        }

        char command[256];
        strncpy(command, buffer, sizeof(command) - 1);
        command[sizeof(command) - 1] = '\0';

        if (strcmp(command, "removef") == 0) { // Check if command is "removef"
    while (1) { // Loop to read file paths
        memset(buffer, 0, BUFFER_SIZE); // Clear buffer for new data
        buf_pos = 0;
        while (buf_pos < BUFFER_SIZE - 1) { 
            bytes_read = read(client, buffer + buf_pos, 1);
            if (bytes_read <= 0) {
                perror("S1: read filepath"); // Print error if read fails
                write(client, "ERROR: Read error\n", 18); // Send error to client
                return;
            }
            if (buffer[buf_pos] == '\n') {
                buffer[buf_pos] = '\0'; // End string at newline
                break;
            }
            buf_pos++;
        }
        if (buf_pos == 0) { // Check if no file path received
            write(client, "No more files\n", 14); // Notify client no more files
            break;
        }
        strncpy(filepath, buffer, sizeof(filepath) - 1); // Copy file path to variable
        filepath[sizeof(filepath) - 1] = '\0'; // Ensure string termination

                char *ext = strrchr(filepath, '.');
                int valid = 0;
                int forward_port = 0;
                char *ip_add = NULL;
                if (ext) {
                    char ext_lower[16];
                    strncpy(ext_lower, ext, sizeof(ext_lower) - 1);
                    ext_lower[sizeof(ext_lower) - 1] = '\0';
                    for (int j = 0; ext_lower[j]; j++) {
                        ext_lower[j] = tolower(ext_lower[j]);
                    }
                    const char *valid_ext[] = {".c", ".pdf", ".txt", ".zip"};
                   for (size_t j = 0; j < sizeof(valid_ext) / sizeof(valid_ext[0]); j++) { // Loop through valid file extensions
    if (strcmp(ext_lower, valid_ext[j]) == 0 && strlen(filepath) > strlen(valid_ext[j])) { // Check if extension matches and file path is valid
        valid = 1; // Mark file as valid
        if (strcmp(ext_lower, ".pdf") == 0) { // Handle PDF files
            forward_port = port_s2;
            ip_add = ip_add_s2;
        } else if (strcmp(ext_lower, ".txt") == 0) { // Handle TXT files
            forward_port = port_s3;
            ip_add = ip_add_s3;
        } else if (strcmp(ext_lower, ".zip") == 0) { // Handle ZIP files
            forward_port = port_s4;
            ip_add = ip_add_s4;
        }
        break; // Exit loop once match is found
    }
}
                }

               if (valid == 0 && ext) { // Check if file type is invalid and has an extension
    printf("S1: Invalid file type: %s\n", filepath); // Log invalid file type
    write(client, "ERROR: Invalid file type\n", 24); 
    continue; 
}

                if (forward_port == 0) { // Check if file should be removed locally
    if (unlink(filepath) == -1) { 
        perror("S1: unlink"); // Print error if deletion fail
        write(client, "ERROR: Failed to remove file\n", 28); 
    } else {
        printf("S1: Removed file %s\n", filepath); 
        write(client, "File removed\n", 13); //sucess
    }
}else { // Handle file removal on another server
    if (forward_remove_to_server(filepath, forward_port, ip_add) == 0) { // Forward removal req
        printf("S1: Forwarded removal of %s to port %d\n", filepath, forward_port); // Log forwarded removal
        write(client, "File removed\n", 13); 
    } else {
        write(client, "ERROR: Failed to remove file\n", 28);
    }
}
            }
        } else if (strcmp(command, "uploadf") == 0) { //handle uploadf 
            memset(buffer, 0, BUFFER_SIZE);
            buf_pos = 0;
            while (buf_pos < BUFFER_SIZE - 1) {
                bytes_read = read(client, buffer + buf_pos, 1); //read one by one 
                if (bytes_read <= 0) {
                    perror("S1: read destination");
                    return;
                }
                if (buffer[buf_pos] == '\n') { //new line 
                    buffer[buf_pos] = '\0';
                    break;
                }
                buf_pos++;//next char 
            }
            if (buf_pos == 0) {
                write(client, "No more paths\n", 14);
                return;
            }
            char destination_path[256]; // Buffer for destination file path
    strncpy(destination_path, buffer, sizeof(destination_path) - 1); // Copy file path from buffer
    destination_path[sizeof(destination_path) - 1] = '\0'; 

            create_full_path(destination_path);

            while (1) {
                memset(buffer, 0, BUFFER_SIZE);
                buf_pos = 0;
                while (buf_pos < BUFFER_SIZE - 1) {
                    bytes_read = read(client, buffer + buf_pos, 1);//one at a time 
                    if (bytes_read <= 0) {
                        perror("S1: read filename");
                        write(client, "ERROR: Read error\n", 18);
                        return;
                    }
                    if (buffer[buf_pos] == '\n') {
                        buffer[buf_pos] = '\0';
                        break;
                    }
                    buf_pos++; //newxt byte 
                }
                if (buf_pos == 0) {
                    write(client, "No more files\n", 14);
                    break;
                }
                char filename[256];
                strncpy(filename, buffer, sizeof(filename) - 1);
                filename[sizeof(filename) - 1] = '\0';

                char *ext = strrchr(filename, '.');
                int valid = 0;
                int forward_port = 0;
                char *ip_add = NULL;
                if (ext) {
                    char ext_lower[16];
                    strncpy(ext_lower, ext, sizeof(ext_lower) - 1);
                    ext_lower[sizeof(ext_lower) - 1] = '\0';
                    for (int j = 0; ext_lower[j]; j++) {
                        ext_lower[j] = tolower(ext_lower[j]);
                    }
                    const char *valid_ext[] = {".c", ".pdf", ".txt", ".zip"}; //exts that need to handle 
                    for (size_t j = 0; j < sizeof(valid_ext) / sizeof(valid_ext[0]); j++) {
                        if (strcmp(ext_lower, valid_ext[j]) == 0 && strlen(filename) > strlen(valid_ext[j])) {
                            valid = 1;
                            if (strcmp(ext_lower, ".pdf") == 0) { //handle pdf 
                                forward_port = port_s2;
                                ip_add = ip_add_s2;
                            } else if (strcmp(ext_lower, ".txt") == 0) { //txt
                                forward_port = port_s3;
                                ip_add = ip_add_s3;
                            } else if (strcmp(ext_lower, ".zip") == 0) { //zips 
                                forward_port = port_s4;
                                ip_add = ip_add_s4;
                            }
                            break;
                        }
                    }
                }

                if (valid == 0 && ext) {
                    printf("S1: Invalid file type: %s\n", filename);
                    write(client, "ERROR: Invalid file type\n", 24);
                    continue;
                }

                char full_file_path[BUFFER_SIZE];
                snprintf(full_file_path, sizeof(full_file_path), "%s/%s", destination_path, filename);

                int file_fd = open(full_file_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if (file_fd < 0) {
                    perror("S1: open");
                    write(client, "ERROR: Failed to create file\n", 28);
                    continue;
                }

                int done_received = 0; // Flag to stop when END_MARKER is found
char end_buffer[END_MARKER_LEN] = {0}; // Stores last few bytes for marker check
int end_pos = 0;

// Read data byte-by-byte until END_MARKER is found
while (!done_received) {
    bytes_read = read(client, buffer, 1);
    if (bytes_read <= 0) { // Error or connection closed
        close(file_fd);
        write(client, "ERROR: Read error\n", 18);
        return;
    }

    end_buffer[end_pos] = buffer[0];
    end_pos++;

    if (end_pos == END_MARKER_LEN) { // Enough bytes to compare
        if (strncmp(end_buffer, END_MARKER, END_MARKER_LEN) == 0) {
            done_received = 1; // Found marker → stop reading
            break;
        }
        memmove(end_buffer, end_buffer + 1, END_MARKER_LEN - 1); // Shift left
        end_pos--;
        write(file_fd, buffer, 1); // Save byte to file
    } else {
        write(file_fd, buffer, 1); // Save byte to file
    }
}


                close(file_fd); //close 
                write(client, "File uploaded\n", 14);

                if (forward_port != 0) {
                    if (forward_file_to_server(filename, destination_path, forward_port, ip_add) == 0) { //forwerad to the server
                        if (unlink(full_file_path) < 0) {
                            perror("S1: unlink");
                        } else {
                            printf("S1: Removed %s after forwarding\n", full_file_path);
                        }
                    } else {
                        fprintf(stderr, "S1: Failed to forward file %s\n", filename);
                    }
                }
            }
        } else if (strcmp(command, "downlf") == 0) { //handle downloadf file
            while (1) {
                memset(buffer, 0, BUFFER_SIZE);
                buf_pos = 0;
                while (buf_pos < BUFFER_SIZE - 1) {
                    bytes_read = read(client, buffer + buf_pos, 1);
                    if (bytes_read <= 0) {
                        perror("S1: read filepath");
                        write(client, "ERROR: Read error\n", 18);
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

                char *ext = strrchr(filepath, '.');
                int valid = 0;
                int forward_port = 0;
                char *ip_add = NULL;
                if (ext) {  
    // Make a lowercase copy of the file extension
    char ext_lower[16];
    strncpy(ext_lower, ext, sizeof(ext_lower) - 1);
    ext_lower[sizeof(ext_lower) - 1] = '\0';
    for (int j = 0; ext_lower[j]; j++) {
        ext_lower[j] = tolower(ext_lower[j]); // Lowercase each character
    }

    // List of extensions we accept
    const char *valid_ext[] = {".c", ".pdf", ".txt", ".zip"};

    // Go through allowed extensions and see if it matches
    for (size_t j = 0; j < sizeof(valid_ext) / sizeof(valid_ext[0]); j++) {
        // Check if file ends with a valid extension 
        if (strcmp(ext_lower, valid_ext[j]) == 0 && strlen(filepath) > strlen(valid_ext[j])) {
            valid = 1; // Mark file as valid

            // Decide which server should get the file based on its extension
            if (strcmp(ext_lower, ".pdf") == 0) {
                forward_port = port_s2; // PDF to S2
                ip_add = ip_add_s2;
            } else if (strcmp(ext_lower, ".txt") == 0) {
                forward_port = port_s3; // TXT to S3
                ip_add = ip_add_s3;
            } else if (strcmp(ext_lower, ".zip") == 0) {
                forward_port = port_s4; // ZIP to S4
                ip_add = ip_add_s4;
            }
            break; 
        }
    }
}


                if (valid == 0 && ext) {
                    printf("S1: Invalid file type: %s\n", filepath);
                    write(client, "ERROR: Invalid file type\n", 24);
                    continue;
                }

                char *filename = strrchr(filepath, '/');
                if (filename == NULL) {
                    filename = filepath;
                } else {
                    filename++;
                }

                if (forward_port == 0) {
                    // Handle .c file locally
                    int file_fd = open(filepath, O_RDONLY);
                    if (file_fd < 0) {
                        perror("S1: open");
                        write(client, "ERROR: File not found\n", 22);
                        continue;
                    }

                    write(client, filename, strlen(filename));
                    write(client, "\n", 1);

                    while ((bytes_read = read(file_fd, buffer, BUFFER_SIZE)) > 0) {
                        write(client, buffer, bytes_read);
                    }
                    if (bytes_read < 0) {
                        perror("S1: read file");
                        write(client, "ERROR: Read error\n", 18);
                        close(file_fd);
                        continue;
                    }
                    write(client, END_MARKER, END_MARKER_LEN);
                    close(file_fd);
                    printf("S1: Sent %s to client\n", filepath);
                } else {
                    // Forward to appropriate server
                    if (forward_download_from_server(filepath, client, forward_port, ip_add) < 0) {
                        write(client, "ERROR: Failed to download file\n", 30);
                    }
                }
            }
        } else {
            printf("S1: Unknown command: %s\n", command);
            write(client, "ERROR: Unknown command\n", 22);
            while (1) {
                bytes_read = read(client, buffer, 1);
                if (bytes_read <= 0 || buffer[0] == '\n') break;
            }
        }
    }
}

int main(int argc, char *argv[]) {
    int sd, client, portNumber;
    struct sockaddr_in servAdd; // Server address structure

    if (argc != 8) { // Check for correct number of arguments
        fprintf(stderr, "Usage: %s <Port#> <S2_IP> <S2_Port> <S3_IP> <S3_Port> <S4_IP> <S4_Port>\n", argv[0]);
        exit(1);
    }

    if ((sd = socket(AF_INET, SOCK_STREAM, 0)) < 0) { // Create socket
        fprintf(stderr, "Could not create the socket\n");
        exit(2);
    }

    servAdd.sin_family = AF_INET; // Set to IPv4
    servAdd.sin_addr.s_addr = htonl(INADDR_ANY); // Accept connections from any IP
    sscanf(argv[1], "%d", &portNumber); // Get port from arguments
    servAdd.sin_port = htons((uint16_t)portNumber); // Set port number

    if (bind(sd, (struct sockaddr *)&servAdd, sizeof(servAdd)) < 0) { // Bind socket to address
        fprintf(stderr, "Bind failed\n");
        close(sd);
        exit(3);
    }

    if (listen(sd, 5) < 0) { // Listen for incoming connections
        fprintf(stderr, "Listen failed\n");
        close(sd);
        exit(4);
    }

    strncpy(ip_add_s2, argv[2], sizeof(ip_add_s2) - 1); // Store IP and port for server S2
    ip_add_s2[sizeof(ip_add_s2) - 1] = '\0';
    sscanf(argv[3], "%d", &port_s2);
    
    strncpy(ip_add_s3, argv[4], sizeof(ip_add_s3) - 1); // Store IP and port for server S3
    ip_add_s3[sizeof(ip_add_s3) - 1] = '\0';
    sscanf(argv[5], "%d", &port_s3);
    
    strncpy(ip_add_s4, argv[6], sizeof(ip_add_s4) - 1); // Store IP and port for server S4
    ip_add_s4[sizeof(ip_add_s4) - 1] = '\0';
    sscanf(argv[7], "%d", &port_s4);

    while (1) { // Main loop to accept client connections
        client = accept(sd, (struct sockaddr*)NULL, NULL); // Accept new client
        if (client < 0) {
            fprintf(stderr, "Client connection failed\n");
            continue;
        }

        int pid = fork(); // Create child process for client
        if (pid == 0) {
            close(sd); // Close server socket in child
            prcclient(client); // Handle client request
            close(client); // Close client socket
            exit(0);
        } else if (pid > 0) { // Parent process
            close(client); 
            waitpid(-1, NULL, WNOHANG); // Clean up terminated child processes
        } else {
            perror("fork"); // Print error if fork fails
        }
    }

    close(sd); // Close server socket
    return 0;
}