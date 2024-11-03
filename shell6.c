#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <signal.h>
#include <errno.h>

#define MAX_LEN 512
#define MAXARGS 10
#define ARGLEN 30
#define PROMPT "PUCITshell:- "
#define HIST_SIZE 10
#define MAX_VARS 100 // Maximum number of variables

typedef struct Job {
    int pid;
    int job_number;
    char command[MAX_LEN];
} Job;

typedef struct Var {
    char *name;
    char *value;
    int global; // 1 for global, 0 for local
} Var;

Job jobs[MAXARGS];
Var variables[MAX_VARS];
int job_count = 0;
char *command_history[HIST_SIZE];
int history_index = 0;

int execute(char *arglist[]);
char **tokenize(char *cmdline);
char *read_cmd();
void add_to_history(char *cmdline);
void repeat_command(char *cmdline);
void sigchld_handler(int signo);
void change_directory(char *path);
void show_jobs();
void kill_job(int job_number);
void kill_job_by_pid(int pid);
void show_help();
void remove_job(int pid);
void set_variable(char *name, char *value, int global);
char *get_variable(char *name);
void list_variables();
void free_variable(int index);

int main() {
    char *cmdline;
    char **arglist;

    // Set up signal handler for SIGCHLD
    struct sigaction sa;
    sa.sa_handler = sigchld_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;
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
            } else if (strcmp(arglist[0], "kill") == 0) {
                if (arglist[1] != NULL) {
                    int pid = atoi(arglist[1]);
                    if (pid > 0) {
                        // Attempt to kill by PID first
                        kill_job_by_pid(pid);
                    } else {
                        // Otherwise, attempt to kill by job number
                        int job_number = atoi(arglist[1]);
                        kill_job(job_number);
                    }
                } else {
                    fprintf(stderr, "Usage: kill <job_number or pid>\n");
                }
            } else if (strcmp(arglist[0], "help") == 0) {
                show_help();
            } else if (strcmp(arglist[0], "set") == 0 && arglist[1] != NULL && arglist[2] != NULL) {
                int global = (arglist[3] != NULL && strcmp(arglist[3], "global") == 0) ? 1 : 0;
                set_variable(arglist[1], arglist[2], global);
            } else if (strcmp(arglist[0], "get") == 0 && arglist[1] != NULL) {
                char *value = get_variable(arglist[1]);
                if (value != NULL) {
                    printf("%s = %s\n", arglist[1], value);
                } else {
                    printf("Variable %s not found\n", arglist[1]);
                }
            } else if (strcmp(arglist[0], "listvars") == 0) {
                list_variables();
            } else {
                execute(arglist);
            }
            for (int j = 0; j < MAXARGS + 1; j++)
                free(arglist[j]);
            free(arglist);
        }
        free(cmdline);
    }
    printf("\n");
    return 0;
}
// Add to command history
void add_to_history(char *cmdline) {
    free(command_history[history_index]);
    command_history[history_index] = strdup(cmdline);
    history_index = (history_index + 1) % HIST_SIZE;
}

// Repeat a command from history
void repeat_command(char *cmdline) {
    int cmd_num;
    if (sscanf(cmdline + 1, "%d", &cmd_num) != 1) {
        fprintf(stderr, "Invalid command number\n");
        return;
    }
    
    if (cmd_num == -1) {
        cmd_num = (history_index - 1 + HIST_SIZE) % HIST_SIZE;
    } else {
        cmd_num = (cmd_num - 1) % HIST_SIZE;
    }
    
    if (command_history[cmd_num] != NULL) {
        printf("%s\n", command_history[cmd_num]);
        char *cmdline = strdup(command_history[cmd_num]);
        
        char **arglist = tokenize(cmdline);
        
        if (arglist != NULL) {
            if (strcmp(arglist[0], "cd") == 0) {
                change_directory(arglist[1]);
            } else {
                execute(arglist);
            }
            for (int j = 0; j < MAXARGS + 1; j++)
                free(arglist[j]);
            free(arglist);
        }
        free(cmdline);
    } else {
        fprintf(stderr, "No command found for that number\n");
    }
}

int execute(char *arglist[]) {
    int status;
    pid_t cpid;
    int i = 0;
    int inRedirect = -1, outRedirect = -1;
    int background = 0;

    while (arglist[i] != NULL) {
        if (strcmp(arglist[i], "<") == 0) {
            inRedirect = i;
        } else if (strcmp(arglist[i], ">") == 0) {
            outRedirect = i;
        }
        i++;
    }

    if (arglist[i - 1] != NULL && strcmp(arglist[i - 1], "&") == 0) {
        background = 1;
        arglist[i - 1] = NULL;
    }

    cpid = fork();
    if (cpid == 0) {
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
        if (background) {
            jobs[job_count].pid = cpid;
            jobs[job_count].job_number = job_count + 1;
            strncpy(jobs[job_count].command, arglist[0], MAX_LEN);
            printf("Started background process with PID %d\n", cpid);
            job_count++;
        } else {
            waitpid(cpid, &status, 0);
            printf("Child exited with status %d\n", status >> 8);
        }
        return 0;
    }
}

void sigchld_handler(int signo) {
    int saved_errno = errno;
    pid_t pid;
    while ((pid = waitpid(-1, NULL, WNOHANG)) > 0) {
        printf("Job with PID %d terminated.\n", pid); // Message when job is terminated
        remove_job(pid);
    }
    errno = saved_errno;
}

char **tokenize(char *cmdline) {
    char **arglist = (char **)malloc(sizeof(char *) * (MAXARGS + 1));
    for (int j = 0; j < MAXARGS + 1; j++) {
        arglist[j] = (char *)malloc(sizeof(char) * ARGLEN);
    }

    int argnum = 0;
    char *start = cmdline;
    while (*start) {
        while (*start == ' ') start++;
        if (*start == '\0') break;

        char *end = start;
        while (*end && *end != ' ') end++;
        int len = end - start;
        if (len > 0) {
            strncpy(arglist[argnum], start, len);
            arglist[argnum][len] = '\0';
            argnum++;
        }
        start = end;
    }
    arglist[argnum] = NULL;
    return arglist;
}

char *read_cmd() {
    char *cmdline = (char *)malloc(sizeof(char) * MAX_LEN);
    printf(PROMPT);
    if (fgets(cmdline, MAX_LEN, stdin) == NULL) {
        free(cmdline);
        return NULL; // EOF
    }
    cmdline[strcspn(cmdline, "\n")] = '\0'; // Remove newline
    return cmdline;
}

void change_directory(char *path) {
    if (path == NULL || chdir(path) != 0) {
        perror("cd failed");
    }
}

void show_jobs() {
    for (int i = 0; i < job_count; i++) {
        printf("[%d] %d %s\n", jobs[i].job_number, jobs[i].pid, jobs[i].command);
    }
}

void kill_job(int job_number) {
    if (job_number > 0 && job_number <= job_count) {
        pid_t pid = jobs[job_number - 1].pid;
        if (kill(pid, SIGKILL) == 0) {
            printf("Killed job [%d] with PID %d: %s\n", job_number, pid, jobs[job_number - 1].command);
            remove_job(pid);
        } else {
            perror("Failed to kill job");
        }
    } else {
        fprintf(stderr, "No such job number\n");
    }
}
void kill_job_by_pid(int pid) {
    if (kill(pid, SIGKILL) == 0) {
        printf("Killed job with PID %d\n", pid);
        remove_job(pid);
    } else {
        perror("Failed to kill job by PID");
    }
}
void remove_job(int pid) {
    for (int i = 0; i < job_count; i++) {
        if (jobs[i].pid == pid) {
            for (int j = i; j < job_count - 1; j++) {
                jobs[j] = jobs[j + 1];
            }
            job_count--;
            return;
        }
    }
}

void show_help() {
    printf("Available commands:\n");
    printf("  cd <path>            - Change directory\n");
    printf("  exit                 - Exit shell\n");
    printf("  jobs                 - Show background jobs\n");
    printf("  kill <job_number>    - Kill job by job number\n");
    printf("  kill <pid>          - Kill job by PID\n");
    printf("  help                 - Show this help message\n");
    printf("  set <name> <value>   - Set variable\n");
    printf("  get <name>           - Get variable value\n");
    printf("  listvars             - List all variables\n");
}

void set_variable(char *name, char *value, int global) {
    for (int i = 0; i < MAX_VARS; i++) {
        if (variables[i].name != NULL && strcmp(variables[i].name, name) == 0) {
            free(variables[i].value);
            variables[i].value = strdup(value);
            return;
        }
        if (variables[i].name == NULL) {
            variables[i].name = strdup(name);
            variables[i].value = strdup(value);
            variables[i].global = global;
            return;
        }
    }
    printf("Error: Maximum number of variables reached\n");
}

char *get_variable(char *name) {
    for (int i = 0; i < MAX_VARS; i++) {
        if (variables[i].name != NULL && strcmp(variables[i].name, name) == 0) {
            return variables[i].value;
        }
    }
    return NULL; // Variable not found
}

void list_variables() {
    printf("User-defined variables:\n");
    for (int i = 0; i < MAX_VARS; i++) {
        if (variables[i].name != NULL) {
            printf("  %s = %s (%s)\n", variables[i].name, variables[i].value, 
                variables[i].global ? "global" : "local");
        }
    }
}

void free_variable(int index) {
    free(variables[index].name);
    free(variables[index].value);
    variables[index].name = NULL;
    variables[index].value = NULL;
}

