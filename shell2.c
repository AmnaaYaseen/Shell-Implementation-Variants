#include "shell_v1.c"  // Use all functionalities from version 1

#include <fcntl.h>
#include <unistd.h>

// Function to handle redirection by detecting < and > symbols
void handle_redirection(char **args) {
    for (int i = 0; args[i] != NULL; i++) {
        if (strcmp(args[i], "<") == 0) { // Input redirection
            int fd = open(args[i + 1], O_RDONLY); // Open file in read-only mode
            dup2(fd, STDIN_FILENO); // Redirect standard input to the file
            close(fd); // Close the file descriptor
            args[i] = NULL; // Nullify this argument so execvp ignores it
        } else if (strcmp(args[i], ">") == 0) { // Output redirection
            int fd = open(args[i + 1], O_WRONLY | O_CREAT | O_TRUNC, 0644); // Open file in write mode
            dup2(fd, STDOUT_FILENO); // Redirect standard output to the file
            close(fd); // Close the file descriptor
            args[i] = NULL; // Nullify this argument so execvp ignores it
        }
    }
}

// Executes a command with redirection capability
void execute_command_v2(char **args) {
    pid_t pid = fork(); // Fork a new process
    if (pid == 0) { // Child process
        handle_redirection(args); // Handle input/output redirection
        execvp(args[0], args); // Execute the command
        perror("Execution failed"); // Error if execvp fails
        exit(EXIT_FAILURE);
    } else if (pid > 0) { // Parent process
        wait(NULL); // Wait for the child process
    }
}

int main() {
    char input[MAX_CMD_LEN];
    char *args[MAX_ARGS];

    while (1) {
        display_prompt();
        if (fgets(input, MAX_CMD_LEN, stdin) == NULL) {
            printf("\nExiting shell...\n");
            break;
        }
        if (parse_command(input, args) > 0) {
            execute_command_v2(args);
        }
    }
    return 0;
}