#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <sys/stat.h>

#define MAX_LEN 512
#define MAXARGS 10
#define ARGLEN 30
#define PROMPT "PUCITshell:- "

int execute(char* arglist[]);
char** tokenize(char* cmdline);
char* read_cmd(char*, FILE*);

int main() {
    char *cmdline;
    char **arglist;
    char *prompt = PROMPT;
    
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

    if (pipeFound != -1) {
        // Handle pipe
        int pipefd[2];
        if (pipe(pipefd) == -1) {
            perror("Pipe failed");
            return 1;
        }

        cpid = fork();
        if (cpid == -1) {
            perror("Fork failed");
            return 1;
        }

        if (cpid == 0) {
            // Child process: execute the first command
            arglist[pipeFound] = NULL;
            close(pipefd[0]);
            dup2(pipefd[1], STDOUT_FILENO);
            close(pipefd[1]);
            execvp(arglist[0], arglist);
            perror("Execution failed");
            exit(1);
        } else {
            // Parent process: execute the second command
            waitpid(cpid, &status, 0);
            cpid = fork();
            if (cpid == 0) {
                dup2(pipefd[0], STDIN_FILENO);
                close(pipefd[1]);
                execvp(arglist[pipeFound + 1], &arglist[pipeFound + 1]);
                perror("Execution failed");
                exit(1);
            } else {
                close(pipefd[0]);
                close(pipefd[1]);
                waitpid(cpid, &status, 0);
            }
        }
        return 0;
    }

    cpid = fork();
    if (cpid == 0) {
        // Handle input redirection
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
        waitpid(cpid, &status, 0);
        printf("Child exited with status %d\n", status >> 8);
        return 0;
    }
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
