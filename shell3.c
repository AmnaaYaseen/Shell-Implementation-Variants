#include "shell2.c"

// Execute command with support for background processes
void execute_command_v3(char **args) {
    int bg = 0; // Flag to check if process is in background
    int len = 0;
    while (args[len] != NULL) len++; // Calculate number of arguments
    if (len > 0 && strcmp(args[len - 1], "&") == 0) { // Check for background '&' symbol
        bg = 1; // Mark as background process
        args[len - 1] = NULL; // Remove '&' from arguments
    }

    pid_t pid = fork(); // Fork a new process
    if (pid == 0) { // Child process
        handle_redirection(args); // Handle redirection if any
        execvp(args[0], args); // Execute the command
        perror("Execution failed");
        exit(EXIT_FAILURE);
    } else if (pid > 0 && !bg) { // Parent process and not in background
        wait(NULL); // Wait for the child process if not background
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
            execute_command_v3(args); // Execute command with background support
        }
    }
    return 0;
}