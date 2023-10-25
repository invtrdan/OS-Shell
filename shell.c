//Authors: Danielle Mcintosh, Nikolas Buckle
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>
#include <limits.h>

#define MAX_COMMAND_LINE_LEN 1024 // Maximum length of a command line
#define MAX_COMMAND_LINE_ARGS 128 // Maximum number of arguments

char prompt[MAX_COMMAND_LINE_LEN] = "> "; // Stores the shell prompt
char delimiters[] = " \t\r\n"; // Contains delimiters used for tokenization
extern char **environ;

// Global flag to indicate whether Ctrl-C was pressed
volatile sig_atomic_t ctrl_c_pressed = 0;

void ctrl_c_handler(int signo) {
    ctrl_c_pressed = 1;
}

// Global flag to indicate that the timer has expired
volatile sig_atomic_t timer_expired = 0;

void timer_handler(int signo) {
    timer_expired = 1;
}

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
    // Set up the Ctrl-C (SIGINT) signal handler
    struct sigaction sa;
    sa.sa_handler = ctrl_c_handler;
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);

    // Set up the timer (alarm) signal handler
    sa.sa_handler = timer_handler;
    sa.sa_flags = 0;
    sigaction(SIGALRM, &sa, NULL);

    char command_line[MAX_COMMAND_LINE_LEN]; // Stores the user's input
    char cmd_bak[MAX_COMMAND_LINE_LEN];
    char *arguments[MAX_COMMAND_LINE_ARGS]; // Stores the tokenized command line input
    	
    while (true) {
        do{ 
            print_shell_prompt(); // Print the shell prompt

            if (ctrl_c_pressed) {
                // Ctrl-C was pressed, reset the flag and continue
                ctrl_c_pressed = 0;
            }

            if (timer_expired) {
                // Timer expired, reset the flag, and kill the child process
                timer_expired = 0;
                kill(0, SIGTERM);  // Send termination signal to the process group
                wait(NULL);  // Clean up the terminated process
                printf("Process terminated due to timeout.\n");
            }

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

        // Check for background process by inspecting the last argument
        bool background = false;
        if(strcmp(arguments[arg_count - 1], "&") == 0) {
            background = true;
            arguments[arg_count - 1] = NULL; // Remove the &
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
            if (!background) {
                // Set a timer before forking a child process
                alarm(10);  // Set a 10-second timer
            }

            bool input_redirected = false;
            bool output_redirected = false;
            int input_fd = -1;
            int output_fd = -1;

            for (int i = 1; i < arg_count; i++) {
                if (strcmp(arguments[i], "<") == 0) {
                    if (i < arg_count - 1) {
                        input_redirected = true;
                        input_fd = open(arguments[i + 1], O_RDONLY);
                        if (input_fd == -1) {
                            perror("open");
                        }
                        arguments[i] = NULL; // Remove the "<" token
                        i++;
                    } else {
                        fprintf(stderr, "Invalid input redirection.\n");
                        break;
                    }
                } else if (strcmp(arguments[i], ">") == 0) {
                    if (i < arg_count - 1) {
                        output_redirected = true;
                        output_fd = open(arguments[i + 1], O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
                        if (output_fd == -1) {
                            perror("open");
                        }
                        arguments[i] = NULL; // Remove the ">" token
                        i++;
                    } else {
                        fprintf(stderr, "Invalid output redirection.\n");
                        break;
                    }
                } else if (strcmp(arguments[i], "|") == 0) {
                    if (i < arg_count - 1) {
                        int pipe_fd[2];
                        if (pipe(pipe_fd) == -1) {
                            perror("pipe");
                            break;
                        }

                        pid_t pipe_pid = fork();
                        if (pipe_pid == -1) {
                            perror("fork");
                        } else if (pipe_pid == 0) {
                            // Child process for piping
                            close(pipe_fd[0]); // Close the reading end
                            if (dup2(pipe_fd[1], STDOUT_FILENO) == -1) {
                                perror("dup2");
                            }
                            close(pipe_fd[1]);
                            execvp(arguments[i + 1], &arguments[i + 1]);
                            perror("execvp");
                            exit(EXIT_FAILURE);
                        } else {
                            // Parent process
                            close(pipe_fd[1]); // Close the writing end
                            if (dup2(pipe_fd[0], STDIN_FILENO) == -1) {
                                perror("dup2");
                            }
                            close(pipe_fd[0]);
                            execvp(arguments[0], arguments);
                            perror("execvp");
                            exit(EXIT_FAILURE);
                        }
                    } else {
                        fprintf(stderr, "Invalid pipe usage.\n");
                        break;
                    }
                }
            }

            // Create a child process to execute the command
            pid_t child_pid = fork();
            if (child_pid == -1) {
                perror("fork");
            } else if (child_pid == 0) {
                // This is the child process
                if (input_redirected) {
                    if (dup2(input_fd, STDIN_FILENO) == -1) {
                        perror("dup2");
                    }
                    close(input_fd);
                }
                if (output_redirected) {
                    if (dup2(output_fd, STDOUT_FILENO) == -1) {
                        perror("dup2");
                    }
                    close(output_fd);
                }
                execvp(arguments[0], arguments);
                perror("execvp");
                exit(EXIT_FAILURE);
            } else {
                // This is the parent process
                if (!background) {
                    int status;
                    if (wait(&status) == -1) { // Parent process waits for the child process to complete
                        perror("wait");
                    }
                    alarm(0);  // Cancel the timer
                }
            }
        }
    }
    // This should never be reached.
    return -1;
}
