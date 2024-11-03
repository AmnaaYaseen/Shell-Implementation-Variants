#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <signal.h>
#include <errno.h>  // Added include for errno

#define MAX_LEN 512
#define MAXARGS 10
#define ARGLEN 30
#define PROMPT "PUCITshell:- "
#define HIST_SIZE 10

typedef struct Job {
    int pid;
    int job_number;
    char command[MAX_LEN];
} Job;

Job jobs[MAXARGS];
int job_count = 0;
char* command_history[HIST_SIZE];
int history_index = 0;

int execute(char* arglist[]);
char** tokenize(char* cmdline);
char* read_cmd();
void add_to_history(char* cmdline);
void repeat_command(char* cmdline);
void sigchld_handler(int signo);
void change_directory(char* path);
void show_jobs();
void kill_job(int job_number);
void show_help();
void remove_job(int pid); // New function to remove job from list

int main() {
    char *cmdline;
    char **arglist;

    // Set up signal handler for SIGCHLD
    struct sigaction sa;
    sa.sa_handler = sigchld_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART | SA_NOCLDSTOP; // Restart if interrupted by handler
    sigaction(SIGCHLD, &sa, NULL);
    
    while (1) {
        cmdline = read_cmd();
        if (cmdline == NULL) {
            break; // Exit on EOF
        }
        if (cmdline[0] == '!') {
            repeat_command(cmdline);
            free(cmdline);
            continue;
        }
        add_to_history(cmdline);
        if ((arglist = tokenize(cmdline)) != NULL) {
            // Check for built-in commands first
            if (strcmp(arglist[0], "cd") == 0) {
                change_directory(arglist[1]);
            } else if (strcmp(arglist[0], "exit") == 0) {
                free(cmdline);
                free(arglist);
                exit(0);
            } else if (strcmp(arglist[0], "jobs") == 0) {
                show_jobs();
            } else if (strcmp(arglist[0], "kill") == 0 && arglist[1] != NULL) {
                int job_number = atoi(arglist[1]);
                // Check if the argument is a PID and handle accordingly
                if (job_number > 0 && job_number <= job_count) {
                    kill_job(job_number); // Kill by job number
                } else {
                    // If not valid job number, treat it as PID
                    int pid = atoi(arglist[1]);
                    if (kill(pid, SIGKILL) == 0) {
                        printf("Killed job with PID %d\n", pid);
                        remove_job(pid); // Remove the job from the jobs list
                    } else {
                        perror("Failed to kill job");
                    }
                }
            } else if (strcmp(arglist[0], "help") == 0) {
                show_help();
            } else {
                execute(arglist);
            }
            // Free allocated memory for arglist
            for (int j = 0; j < MAXARGS + 1; j++)
                free(arglist[j]);
            free(arglist);
        }
        free(cmdline);
    }
    printf("\n");
    return 0;
}

void add_to_history(char* cmdline) {
    free(command_history[history_index]); // Free previous command
    command_history[history_index] = strdup(cmdline); // Store the new command
    history_index = (history_index + 1) % HIST_SIZE; // Circular increment
}

void repeat_command(char* cmdline) {
    int cmd_num;
    if (sscanf(cmdline + 1, "%d", &cmd_num) != 1) {
        fprintf(stderr, "Invalid command number\n");
        return;
    }
    
    // Convert -1 to the last command
    if (cmd_num == -1) {
        cmd_num = (history_index - 1 + HIST_SIZE) % HIST_SIZE; // Get last command
    } else {
        cmd_num = (cmd_num - 1) % HIST_SIZE; // Adjust for zero-indexed array
    }
    
    if (command_history[cmd_num] != NULL) {
        printf("%s\n", command_history[cmd_num]); // Print the command being repeated
        char* cmdline = strdup(command_history[cmd_num]);
        
        // Declare arglist here
        char** arglist = tokenize(cmdline); // Ensure arglist is declared before use
        
        if (arglist != NULL) {
            // Check for built-in commands first
            if (strcmp(arglist[0], "cd") == 0) {
                change_directory(arglist[1]);
            } else {
                execute(arglist);
            }
            // Free allocated memory for arglist
            for (int j = 0; j < MAXARGS + 1; j++)
                free(arglist[j]);
            free(arglist);
        }
        free(cmdline);
    } else {
        fprintf(stderr, "No command found for that number\n");
    }
}

int execute(char* arglist[]) {
    int status;
    pid_t cpid;
    int i = 0;
    int inRedirect = -1, outRedirect = -1;
    int background = 0;

    // Check for redirection or pipe symbols
    while (arglist[i] != NULL) {
        if (strcmp(arglist[i], "<") == 0) {
            inRedirect = i;
        } else if (strcmp(arglist[i], ">") == 0) {
            outRedirect = i;
        }
        i++;
    }

    // Check if the last argument is '&' for background process
    if (arglist[i - 1] != NULL && strcmp(arglist[i - 1], "&") == 0) {
        background = 1;
        arglist[i - 1] = NULL; // Remove '&' from arglist
    }

    cpid = fork();
    if (cpid == 0) {
        // Child process
        if (inRedirect != -1) {
            int fd = open(arglist[inRedirect + 1], O_RDONLY);
            if (fd < 0) {
                perror("Failed to open file for reading");
                exit(1);
            }
            dup2(fd, STDIN_FILENO);
            close(fd);
            arglist[inRedirect] = NULL;
        }

        // Handle output redirection
        if (outRedirect != -1) {
            int fd = open(arglist[outRedirect + 1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (fd < 0) {
                perror("Failed to open file for writing");
                exit(1);
            }
            dup2(fd, STDOUT_FILENO);
            close(fd);
            arglist[outRedirect] = NULL;
        }

        execvp(arglist[0], arglist);
        perror("Command not found...");
        exit(1);
    } else {
        // Parent process
        if (background) {
            jobs[job_count].pid = cpid;
            jobs[job_count].job_number = job_count + 1;
            strncpy(jobs[job_count].command, arglist[0], MAX_LEN);
            printf("Started background process with PID %d\n", cpid);
            job_count++;
            // No need to wait for background processes
        } else {
            waitpid(cpid, &status, 0);
            printf("Child exited with status %d\n", status >> 8);
        }
        return 0;
    }
}

void sigchld_handler(int signo) {
    // Wait for all dead processes
    int saved_errno = errno; // Save errno before calling waitpid
    pid_t pid;
    
    while ((pid = waitpid(-1, NULL, WNOHANG)) > 0) {
        remove_job(pid); // Remove the job from the list
    }
    
    errno = saved_errno; // Restore errno after
}

char** tokenize(char* cmdline) {
    // Allocate memory
    char** arglist = (char**)malloc(sizeof(char*) * (MAXARGS + 1));
    for (int j = 0; j < MAXARGS + 1; j++) {
        arglist[j] = (char*)malloc(sizeof(char) * ARGLEN);
    }

    int argnum = 0;
    char* start = cmdline;
    while (*start) {
        // Skip spaces
        while (*start == ' ') start++;
        if (*start == '\0') break; // End of line
        
        char* end = start;
        while (*end && *end != ' ') end++;
        
        // Copy the argument to arglist
        int len = end - start;
        if (len >= ARGLEN) len = ARGLEN - 1; // Prevent overflow
        if (len) {
            strncpy(arglist[argnum++], start, len);
            arglist[argnum - 1][len] = '\0';  // Null-terminate the string
        }
        start = end; // Move to the next argument
    }
    arglist[argnum] = NULL;  // Null-terminate the list of arguments
    return arglist;
}

char* read_cmd() {
    char* cmdline = (char*)malloc(sizeof(char) * MAX_LEN);
    printf(PROMPT);
    if (fgets(cmdline, MAX_LEN, stdin) == NULL) {
        free(cmdline);
        return NULL; // Return NULL on EOF
    }
    // Remove the newline character
    cmdline[strcspn(cmdline, "\n")] = '\0';
    return cmdline;
}

void change_directory(char* path) {
    if (path == NULL) {
        fprintf(stderr, "cd: No path specified\n");
        return;
    }
    if (chdir(path) != 0) {
        perror("cd failed");
    }
}

void show_jobs() {
    if (job_count == 0) {
        printf("No background jobs\n");
        return;
    }
    for (int i = 0; i < job_count; i++) {
        printf("[%d] %d %s\n", jobs[i].job_number, jobs[i].pid, jobs[i].command);
    }
}

void kill_job(int job_number) {
    if (job_number > 0 && job_number <= job_count) {
        int pid = jobs[job_number - 1].pid; // Get the PID of the job
        if (kill(pid, SIGKILL) == 0) {
            printf("Killed job [%d]: %d\n", job_number, pid);
            remove_job(pid); // Remove from job list
        } else {
            perror("Failed to kill job");
        }
    } else {
        fprintf(stderr, "Invalid job number\n");
    }
}

void show_help() {
    printf("Available commands:\n");
    printf("cd <path>       Change directory\n");
    printf("exit            Exit the shell\n");
    printf("jobs            Show background jobs\n");
    printf("kill <job_num>  Kill the specified job\n");
    printf("!<cmd_num>      Repeat a command from history\n");
    printf("help            Show this help message\n");
}

void remove_job(int pid) {
    for (int i = 0; i < job_count; i++) {
        if (jobs[i].pid == pid) {
            // Shift jobs down
            for (int j = i; j < job_count - 1; j++) {
                jobs[j] = jobs[j + 1];
            }
            job_count--;
            printf("Removed job with PID %d\n", pid);
            return;
        }
    }
}
