//Authors: Danielle Mcintosh, Nikolas Buckle
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>
#include <limits.h>

#define MAX_COMMAND_LINE_LEN 1024 // Maximum length of a command line
#define MAX_COMMAND_LINE_ARGS 128 // Maximum number of arguments

char prompt[MAX_COMMAND_LINE_LEN] = "> "; // Stores the shell prompt
char delimiters[] = " \t\r\n"; // Contains delimiters used for tokenization
extern char **environ;

void print_shell_prompt() {
    char cwd[PATH_MAX];
    if (getcwd(cwd, sizeof(cwd)) != NULL) {
        snprintf(prompt, sizeof(prompt), "%s> ", cwd);
    } else {
        perror("getcwd");
        strcpy(prompt, "> ");
    }
    printf("%s", prompt);
}

int main() {
    char command_line[MAX_COMMAND_LINE_LEN]; // Stores the user's input
    char cmd_bak[MAX_COMMAND_LINE_LEN];
    char *arguments[MAX_COMMAND_LINE_ARGS]; // Stores the tokenized command line input
    	
    while (true) {
      
        do{ 
            print_shell_prompt(); // Print the shell prompt

            // Read input from stdin and store it in command_line. If there's an error, exit immediately. 
            if ((fgets(command_line, sizeof(command_line), stdin) == NULL) && ferror(stdin)) {
                fprintf(stderr, "fgets error");
                perror("fgets error");
                exit(EXIT_FAILURE);
            }
        } while (command_line[0] == '\n'); // Handle empty lines

        // If the user input was EOF (ctrl+d), exit the shell.
        if (feof(stdin)) {
            printf("\n");
            fflush(stdout);
            fflush(stderr);
            return 0;
        }

        // Tokenize the command line input (split it on whitespace)
        int arg_count = 0;
        char *token = strtok(command_line, delimiters);
        while (token != NULL && arg_count < MAX_COMMAND_LINE_ARGS - 1) {
            arguments[arg_count] = token;
            arg_count++;
            token = strtok(NULL, delimiters);
        }
        arguments[arg_count] = NULL;

        if (arg_count == 0) {
            continue; // Empty command line, prompt again.
        }

        // Implement Built-In Commands
        if (strcmp(arguments[0], "cd") == 0) {
            if (arg_count == 1) {
                // No argument provided, change to the user's home directory
                if (chdir(getenv("HOME")) != 0) {
                    perror("chdir");
                }
            } else {
                if (chdir(arguments[1]) != 0) {
                    perror("chdir");
                }
            }
        } else if (strcmp(arguments[0], "pwd") == 0) {
            char cwd[PATH_MAX];
            if (getcwd(cwd, sizeof(cwd)) != NULL) {
                printf("%s\n", cwd);
            } else {
                perror("getcwd");
            }
        } else if (strcmp(arguments[0], "echo") == 0) {
            for (int i = 1; i < arg_count; i++) {
                printf("%s ", arguments[i]);
            }
            printf("\n");
        } else if (strcmp(arguments[0], "exit") == 0) {
            exit(0);
        } else if (strcmp(arguments[0], "env") == 0) {
            for (char **env = environ; *env != NULL; env++) {
                printf("%s\n", *env);
            }
        } else if (strcmp(arguments[0], "setenv") == 0) {
            if (arg_count != 3) {
                printf("Usage: setenv VARIABLE VALUE\n");
            } else {
                if (setenv(arguments[1], arguments[2], 1) != 0) {
                    perror("setenv");
                }
            }
        } else {
            // Create a child process to execute the command
            pid_t child_pid = fork();
            if (child_pid == -1) {
                perror("fork");
            } else if (child_pid == 0) {
                // This is the child process
                if (execvp(arguments[0], arguments) == -1) {
                    perror("execvp");
                    exit(EXIT_FAILURE);
                }
            } else {
                // This is the parent process
                int status;
                if (wait(&status) == -1) { // Parent process waits for the child process to complete
                    perror("wait");
                }
            }
        }
        
    }
    // This should never be reached.
    return -1;
}
