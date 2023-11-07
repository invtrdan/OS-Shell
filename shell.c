// Authors: Danielle Mcintosh, Nikolas Buckle
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
        strcpy(prompt, "> "); // Corrected strncpy to strcpy since the fallback prompt is small enough
    }
    printf("%s", prompt);
}

// This function initializes the sigaction struct
void initialize_sigaction(struct sigaction *sa, void (*handler)(int)) {
    memset(sa, 0, sizeof(struct sigaction)); // Initialize the structure to zero
    sa->sa_handler = handler; // Set the signal handler function
    sa->sa_flags = 0; // No flags set
    // Initialize the signal set to empty, to not block additional signals during the handler
    sigemptyset(&sa->sa_mask);
}

int main() {
    // Set up the Ctrl-C (SIGINT) and timer (SIGALRM) signal handlers
    struct sigaction sa_int, sa_alrm;

    // Initialize signal actions
    initialize_sigaction(&sa_int, ctrl_c_handler);
    initialize_sigaction(&sa_alrm, timer_handler);

    // Apply the signal handlers
    sigaction(SIGINT, &sa_int, NULL);
    sigaction(SIGALRM, &sa_alrm, NULL);

    char command_line[MAX_COMMAND_LINE_LEN]; // Stores the user's input
    char *arguments[MAX_COMMAND_LINE_ARGS]; // Stores the tokenized command line input

    while (true) {
        do {
            print_shell_prompt(); // Print the shell prompt

            // Clear input buffer to handle Ctrl-C during fgets
            if (ctrl_c_pressed) {
                // Ctrl-C was pressed, reset the flag and continue
                ctrl_c_pressed = 0;
                // Clear the input buffer to prevent fgets from reading in the interrupted input
                while ((getchar()) != '\n');
                continue; // Continue to the next loop iteration to prompt again
            }

            // Read input from stdin and store it in command_line. If there's an error, exit immediately.
            if (!fgets(command_line, sizeof(command_line), stdin)) {
                // If fgets fails due to CTRL-D (EOF) or an error, handle accordingly
                if (feof(stdin)) {
                    printf("\n");
                    fflush(stdout);
                    fflush(stderr);
                    exit(EXIT_SUCCESS);
                } else if (ferror(stdin)) {
                    perror("fgets");
                    exit(EXIT_FAILURE);
                }
            }
        } while (command_line[0] == '\n'); // Handle empty lines

        // Check if fgets read an EOF (ctrl+d)
        if (feof(stdin)) {
            printf("\n");
            exit(EXIT_SUCCESS);
        }

        // Check for background process request and handle it before strtok modification
        bool background = (command_line[strlen(command_line) - 2] == '&');

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

        // Background process handling: Remove the '&' from arguments if present
        if (background) {
            arg_count--; // Decrement the count of arguments
            arguments[arg_count] = NULL; // Remove the '&' from arguments
        }

        pid_t child_pid = fork(); // Create a new process
        if (child_pid < 0) {
            perror("fork"); // Print an error message if fork fails
            exit(EXIT_FAILURE);
        }

        if (child_pid == 0) {
            // Child process

            // Handle Ctrl+C (SIGINT) signal
            sa_int.sa_handler = SIG_DFL;
            sigaction(SIGINT, &sa_int, NULL);

            // Execute the command
            if (execvp(arguments[0], arguments) < 0) {
                perror("execvp");
                exit(EXIT_FAILURE);
            }
        } else {
            // Parent process

            if (!background) {
                // Only set the alarm if the process is not a background process
                alarm(10);

                // Wait for the child process to complete
                int status;
                waitpid(child_pid, &status, 0);

                // Cancel the alarm if the child process completes before the timer expires
                alarm(0);
            }
        }
    }

    return 0;
}
