#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <signal.h>

#define MAX_LEN 512
#define MAXARGS 10
#define ARGLEN 30
#define PROMPT "PUCITshell:- "

int execute(char* arglist[]);
char** tokenize(char* cmdline);
char* read_cmd(char*, FILE*);
void sigchld_handler(int signo);

int main() {
    char *cmdline;
    char **arglist;
    char *prompt = PROMPT;

    // Set up signal handler for SIGCHLD
    struct sigaction sa;
    sa.sa_handler = sigchld_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART | SA_NOCLDSTOP; // Restart if interrupted by handler
    sigaction(SIGCHLD, &sa, NULL);
    
    while ((cmdline = read_cmd(prompt, stdin)) != NULL) {
        if ((arglist = tokenize(cmdline)) != NULL) {
            execute(arglist);
            // Free allocated memory for arglist
            for (int j = 0; j < MAXARGS + 1; j++)
                free(arglist[j]);
            free(arglist);
            free(cmdline);
        }
    }
    printf("\n");
    return 0;
}

int execute(char* arglist[]) {
    int status;
    pid_t cpid;
    int i = 0;
    int inRedirect = -1, outRedirect = -1, pipeFound = -1;
    int background = 0;

    // Check for redirection or pipe symbols
    while (arglist[i] != NULL) {
        if (strcmp(arglist[i], "<") == 0) {
            inRedirect = i;
        } else if (strcmp(arglist[i], ">") == 0) {
            outRedirect = i;
        } else if (strcmp(arglist[i], "|") == 0) {
            pipeFound = i;
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
            printf("Started background process with PID %d\n", cpid);
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
    while (waitpid(-1, NULL, WNOHANG) > 0);
}

char** tokenize(char* cmdline) {
    // Allocate memory
    char** arglist = (char**)malloc(sizeof(char*) * (MAXARGS + 1));
    for (int j = 0; j < MAXARGS + 1; j++) {
        arglist[j] = (char*)malloc(sizeof(char) * ARGLEN);
        bzero(arglist[j], ARGLEN);
    }

    if (cmdline[0] == '\0')  // if user has entered nothing and pressed enter key
        return NULL;

    int argnum = 0;  // slots used
    char* cp = cmdline;  // pos in string
    char* start;
    int len;

    while (*cp != '\0') {
        while (*cp == ' ' || *cp == '\t')  // skip leading spaces
            cp++;
        start = cp;  // start of the word
        len = 1;
        // find the end of the word
        while (*++cp != '\0' && !(*cp == ' ' || *cp == '\t'))
            len++;
        strncpy(arglist[argnum], start, len);
        arglist[argnum][len] = '\0';
        argnum++;
    }
    arglist[argnum] = NULL;
    return arglist;
}

char* read_cmd(char* prompt, FILE* fp) {
    printf("%s", prompt);
    int c;  // input character
    int pos = 0;  // position of character in cmdline
    char* cmdline = (char*)malloc(sizeof(char) * MAX_LEN);

    while ((c = getc(fp)) != EOF) {
        if (c == '\n')
            break;
        cmdline[pos++] = c;
    }
    // these two lines are added, in case user presses ctrl+d to exit the shell
    if (c == EOF && pos == 0)
        return NULL;
    cmdline[pos] = '\0';
    return cmdline;
}
