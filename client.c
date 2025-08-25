#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>

#define BUFFER_SIZE 4096
#define END_MARKER "END_OF_FILE"
#define END_MARKER_LEN 11
#define RES_L 50 // Max bytes to read for server response

// Function to upload files to server
void upload_files(int server, char **filenames, int file_count, char *destination_path) {
    char buffer[BUFFER_SIZE]; // Buffer for reading file data
    ssize_t bytes_read;      // Track bytes read from file

    // Tell server we want to upload files
    write(server, "uploadf", 7); // Send "uploadf" command
    write(server, "\n", 1);       // Send newline

    // Send where to save files
    write(server, destination_path, strlen(destination_path)); // Send destination path
    write(server, "\n", 1);                                   // Send newline

    // Loop through each file
    for (int i = 0; i < file_count; i++) {
        // Send filename
        write(server, filenames[i], strlen(filenames[i])); // Send name of file
        write(server, "\n", 1);                           // Send newline

        // Open file for reading
        int file_fd = open(filenames[i], O_RDONLY);
        if (file_fd < 0) { // If file can't be opened
            perror("open"); // Show error
            free(filenames[i]); // Free filename memory
            continue; // Skip to next file
        }

        // Send file content one byte at a time
        while ((bytes_read = read(file_fd, buffer, 1)) > 0) {
            write(server, buffer, bytes_read); // Send each byte
        }
        if (bytes_read < 0) { // If reading file fails
            perror("read"); // Show error
            close(file_fd); // Close file
            free(filenames[i]); // Free filename memory
            continue; // Skip to next file
        }

        // Mark end of file
        write(server, END_MARKER, END_MARKER_LEN); // Send end marker
        close(file_fd); // Close file

        // Read server response
        int buf_pos = 0; // Start at beginning of buffer
        buffer[0] = '\0';            // Make sure buffer starts empty
        while (buf_pos < RES_L - 1) { 
            bytes_read = read(server, buffer + buf_pos, 1); // Read one byte from server
            if (bytes_read <= 0) {  // If no bytes read or error
                printf("No response from server for %s\n", filenames[i]);
                break;
            }
            if (buffer[buf_pos] == '\n') {   // If we get a newline
                buffer[buf_pos] = '\0';      // End the string
                break;              
            }
            buf_pos++;                      // Move to next position in buffer
        }
        printf("Server response for %s: %s\n", filenames[i], buffer); // Print server response
        free(filenames[i]); // Free filename memory
    }

   
    write(server, "\n", 1); // Send empty line

    // Read final server message
    int buf_pos = 0; // Start at beginning of buffer
    buffer[0] = '\0'; // Clear buffer
    while (buf_pos < RES_L - 1) { 
        bytes_read = read(server, buffer + buf_pos, 1); // Read one byte
        if (bytes_read <= 0) {
            break; // Stop reading
        }
        if (buffer[buf_pos] == '\n') { // If newline found
            buffer[buf_pos] = '\0'; // End the string
            break; 
        }
        buf_pos++; // Move to next position
    }
}

void download_files(int server, char **filepaths, int file_count) {
    char buffer[BUFFER_SIZE];
    ssize_t bytes_read;

    write(server, "downlf", 6); // send command name
    write(server, "\n", 1);

    // Send file paths
    for (int i = 0; i < file_count; i++) {
        write(server, filepaths[i], strlen(filepaths[i]));
        write(server, "\n", 1);
    }
    // Send empty line to signal end of file paths
    write(server, "\n", 1);

    // Receive files
    for (int i = 0; i < file_count; i++) {
        // Read server response (filename or error)
        int buf_pos = 0;
        buffer[0] = '\0';
        while (buf_pos < RES_L - 1) {
            bytes_read = read(server, buffer + buf_pos, 1);
            if (bytes_read <= 0) {
                printf("No response from server for %s\n", filepaths[i]);
                return;
            }
            if (buffer[buf_pos] == '\n') {
                buffer[buf_pos] = '\0';
                break;
            }
            buf_pos++;
        }

        if (strncmp(buffer, "ERROR", 5) == 0) {
            printf("Server error for %s: %s\n", filepaths[i], buffer);
            continue;
        }

        // Extract filename from path
        char *filename = strrchr(filepaths[i], '/');
        if (filename == NULL) {
            filename = filepaths[i];
        } else {
            filename++;
        }

        // Open file for writing
        int file_fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (file_fd < 0) { //if fd is less than 0 then there is some errorr
            perror("open"); //
            continue;
        }

        // Receive file content
        while (1) {
            bytes_read = read(server, buffer, BUFFER_SIZE);
            if (bytes_read <= 0) {
                printf("Error reading file content for %s\n", filename);
                break;
            }

            // Check for end marker
            if (bytes_read >= END_MARKER_LEN && 
                strncmp(buffer + bytes_read - END_MARKER_LEN, END_MARKER, END_MARKER_LEN) == 0) {
                // Write content before end marker
                write(file_fd, buffer, bytes_read - END_MARKER_LEN);
                break;
            }
            write(file_fd, buffer, bytes_read);
        }

        close(file_fd);
        printf("Downloaded %s\n", filename);
    }
}

// Get just the filename from a file path
char *get_filename(const char *filepath) {
    char *filename = strrchr(filepath, '/'); // Look for last '/' in path
    if (filename == NULL) {                
        printf("Please provide file path with filename\n"); 
        return (char *)filepath;            // Return whole path as filename
    }
    return filename + 1;                    // Return filename after last '/'
}

void remove_files(int server, char **filename, int file_count, char *filepath[]) {
    char buffer[BUFFER_SIZE];
    ssize_t bytes_read;

    for (int i = 0; i < file_count; i++) {
        write(server, "removef", 7); // send command name
        write(server, "\n", 1);

        write(server, filepath[i], strlen(filepath[i])); // send file path with name
        write(server, "\n", 1);

        write(server, filename[i], strlen(filename[i])); // send file name
        write(server, "\n", 1);

        // Read server response
        int buf_pos = 0;
        buffer[0] = '\0';
        while (buf_pos < RES_L - 1) {
            bytes_read = read(server, buffer + buf_pos, 1);
            if (bytes_read <= 0) {
                printf("No response from server for %s\n", filename[i]);
                break;
            }
            if (buffer[buf_pos] == '\n') {
                buffer[buf_pos] = '\0';
                break;
            }
            buf_pos++;
        }
        printf("Server response for %s: %s\n", filename[i], buffer);
    }
}

int main(int argc, char *argv[]) {
  // Check if correct number of arguments (IP and port) provided
if (argc != 3) {
    printf("Usage: %s <IP> <Port#>\n", argv[0]); // Show how to use program
    exit(1);   
}

// Create a TCP socket
int server = socket(AF_INET, SOCK_STREAM, 0);
if (server < 0) {         // If socket creation fails
    perror("socket");                    // Print error
    exit(1);                                    // Quit program
}

// Set up server address details
struct sockaddr_in servAdd;
servAdd.sin_family = AF_INET; // Use IPv4
servAdd.sin_port = htons(atoi(argv[2])); // Convert port number to network format
inet_pton(AF_INET, argv[1], &servAdd.sin_addr); // Convert IP string to binary

// Try to connect to server
if (connect(server, (struct sockaddr*)&servAdd, sizeof(servAdd)) < 0) {
    perror("connect");                          // Print error if connection fails
    exit(1);                                    // Quit program
}

    char input[BUFFER_SIZE];
    while (1) {
        printf("s25client$: ");
        if (fgets(input, BUFFER_SIZE, stdin) == NULL) {
            break; 
        }
        input[strcspn(input, "\n")] = 0; // Remove newline

        // Check for quit command
        if (strcmp(input, "quit") == 0) {
            write(server, "\n", 1); // Send empty path to signal server to exit
            break;
        }

        // Parse input for command and arguments
        char input_copy[BUFFER_SIZE];
        strncpy(input_copy, input, BUFFER_SIZE - 1);
        input_copy[BUFFER_SIZE - 1] = '\0';

        char *token = strtok(input_copy, " ");
        char *command = token;
        char *tokens[3]; // Store up to 2 file paths + 1 for command
        int token_count = 0;

        // Collect all tokens
        token = strtok(NULL, " ");
        while (token != NULL && token_count < 2) {
            tokens[token_count] = token;
            token_count++;
            token = strtok(NULL, " ");
        }

        // Handle upload command
if (command && strcmp(command, "uploadf") == 0) {
    // Check if at least one file and destination path provided
    if (token_count < 2) {
        printf("Error: At least one filename and a destination path required\n"); // Show error if not enough arguments
        continue; // Skip to next command
    }

    // Last token is the destination path
    char *destination_path = tokens[token_count - 1]; // Get destination path
    int file_count = token_count - 1; // Number of files to upload
    // Allocate memory for filenames array
    char **filenames = malloc(file_count * sizeof(char *));
    // Copy each filename
    for (int i = 0; i < file_count; i++) {
        filenames[i] = strdup(tokens[i]); // Copy filename
        if (!filenames[i]) { // If copy fails
            perror("strdup"); 
            
            for (int j = 0; j < i; j++) {
                free(filenames[j]);
            }
            free(filenames); // Free the array
            continue; // Skip to next command
        }
    }

    // Call function to upload files
    upload_files(server, filenames, file_count, destination_path);
    free(filenames); // Free the filenames array
}
        else if (command && strcmp(command, "removef") == 0) { //compare the arg 
            if (token_count < 1 || token_count > 2) {
                printf("Error: removef 1 or 2 file paths\n");
                continue;
            }

            char *filenames[2];
            for (int i = 0; i < token_count; i++) {
                filenames[i] = get_filename(tokens[i]); // Extract filename from each path
            }

            remove_files(server, filenames, token_count, tokens);
        }
        else if (command && strcmp(command, "downlf") == 0) {
            if (token_count < 1 || token_count > 2) {
                printf("Error: downlf requires 1 or 2 file paths\n");
                continue;
            }

            char **filepaths = malloc(token_count * sizeof(char *));
            for (int i = 0; i < token_count; i++) {
                filepaths[i] = strdup(tokens[i]);
                if (!filepaths[i]) {
                    perror("strdup");
                    for (int j = 0; j < i; j++) {
                        free(filepaths[j]);
                    }
                    free(filepaths);
                    continue;
                }
            }

            // Call function to download files from server
download_files(server, filepaths, token_count);
// Free each copied file path
for (int i = 0; i < token_count; i++) {
    free(filepaths[i]); // Release memory for each file path
}
// Free the file paths array
free(filepaths);
        }
        else {
            printf("Unknown command: ");
        }
    }

    close(server); //close conn
    return 0;
}