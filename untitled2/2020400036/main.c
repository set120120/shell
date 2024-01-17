#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <time.h>


/************************* DECLARATIONS ***************************/

#define ALIAS_FILE "aliases.txt"
#define MAX_INPUT_SIZE 512 // I decided max input size is 512.
#define BELLO_FILE "bello.txt"

int flag = 1;
int* pointerOfFlag = &flag;

char LAST_INPUT[512];

void execute_command(char *command);

void load_aliases();

void bello(char *input);

void save_aliases();

char *replace_alias(char *input);

void handle_alias(char *alias_command);

/*********************************************************************/

// Alias struct
struct Alias {
    char name[MAX_INPUT_SIZE];
    char command[MAX_INPUT_SIZE];
};
struct Alias aliases[MAX_INPUT_SIZE];
int alias_count = 0;

int main() {
    load_aliases();

    char input[MAX_INPUT_SIZE];

    while (1) {
        // set flag to true
        *pointerOfFlag = 1;

        // Print the command prompt with username, hostname, and current working directory
        char cwd[512];
        char *username = getlogin();
        char hostname[512];
        getcwd(cwd, sizeof(cwd));
        gethostname(hostname, sizeof(hostname));
        printf("%s@%s %s --- ", username, hostname, cwd);

        // Read user input from the console
        fgets(input, sizeof(input), stdin);

        // Remove the newline character from the input
        input[strcspn(input, "\n")] = 0;

        // Check for exit or quit commands
        if (strcmp(input, "exit") == 0 || strcmp(input, "quit") == 0) {
            save_aliases(); // Save aliases before exiting
            break;
        }
        else if (strncmp(input, "alias", 5) == 0) {
            handle_alias(input); // Handle alias commands
            strcpy(LAST_INPUT, input);
        }
        else if (strcmp(input, "cd") == 0) {
            printf("Invalid argument.\n"); // Print an error message for invalid 'cd' command
        }
        else if (strncmp(input, "cd", 2) == 0) {
            // Change the current working directory
            char *newPath = input + 3;
            chdir(newPath);
            strcpy(LAST_INPUT, input);
        }
        else if (strncmp(input, "bello", 5) == 0) {
            // Execute the 'bello' command and display information
            bello(input);
            strcpy(LAST_INPUT, input);
        }
        else {
            // Check if the command should run in the background
            int run_in_background = 0;
            if (input[strlen(input) - 1] == '&') {
                run_in_background = 1;
                // Remove "&"
                strcpy(LAST_INPUT, input);
                input[strlen(input) - 1] = '\0';
            }

            // Check for aliases and replace them in the input
            char *processed_input = replace_alias(input);

            if (run_in_background) {
                // Execute the command in the background
                pid_t bg_pid = fork();
                if (bg_pid == 0) {
                    // child
                    execute_command(processed_input);
                    free(processed_input); // Free allocated memory above
                    //continue;
                    exit(EXIT_SUCCESS);  //If code does not replace with "continue".
                }
                else if (bg_pid < 0) {
                    perror("Background process fork error");
                }
                else {
                    // Parent
                    usleep(150000);
                    //printf("Background process %d started\n", bg_pid);
                }
            }
            else {
                // Execute the command in the foreground
                execute_command(processed_input);
                free(processed_input); // Free allocated memory above
            }
        }
        // Save the last input
//        if (*pointerOfFlag) {
//            strcpy(LAST_INPUT, input);
//        }
    }

    return 0;
}
// I execute my commands with that function
void execute_command(char *command) {
    pid_t pid = fork();

    if (pid == 0) {
        // child process

        // Tokenize the command into arguments
        char *args[MAX_INPUT_SIZE];
        char *token = strtok(command, " ");
        int i = 0;

        while (token != NULL) {
            args[i++] = token;
            token = strtok(NULL, " ");
        }

        args[i] = NULL;

        // Check for I/O redirection
        int redirect_index = -1;
        for (int j = 0; j < i; j++) {
            if (strcmp(args[j], ">") == 0 || strcmp(args[j], ">>") == 0 || strcmp(args[j], ">>>") == 0) {
                redirect_index = j;
                break;
            }
        }

        if (redirect_index != -1) {
            // If redirection will be done
            char *filename = args[redirect_index + 1];
            int fd;

            if (strcmp(args[redirect_index], ">") == 0) {
                // Output redirection (>), overwrite the file
                fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0666);
            }
            else {
                // Output redirection (>> and >>>), append to the file
                fd = open(filename, O_WRONLY | O_CREAT | O_APPEND, 0666);
            }

            if (fd == -1) {
                // Handling file opening error
                perror("Error opening file");
                exit(EXIT_FAILURE);
            }

            if (strcmp(args[redirect_index], ">>>") == 0) {
                // Reverse the output and write to the file for ">>>"
                dup2(fd, STDOUT_FILENO);
                close(fd); // Close the file
                int len = strlen(args[redirect_index - 1]);
                for (int k = len - 1; k >= 0; k--) {
                    printf("%c", args[redirect_index - 1][k]);
                }
                printf("\n");
                exit(EXIT_SUCCESS);
            } else {
                // Other redirections
                dup2(fd, STDOUT_FILENO);
                close(fd);
            }

            args[redirect_index] = NULL; // Remove the redirection symbol
        }

        // Execute the command
        if (strcmp(args[0], "echo") == 0 && args[1] != NULL && args[1][0] == '"') {
            // Special handling for echo command to prevent printing double quotes
            for (int k = 1; args[1][k] != '\0'; k++) {
                if (args[1][k] == '"') {
                    // Replace the closing quote with null character
                    args[1][k] = '\0';
                    break;
                }
                printf("%c", args[1][k]);
            }
            printf("\n");
            exit(EXIT_SUCCESS);
        } else if (execvp(args[0], args) == -1) {
            perror("Command not found");
            exit(EXIT_FAILURE);
        }
        strcpy(LAST_INPUT, command);
    } else if (pid < 0) {
        // Handle fork failure
        perror("Error");
    } else {
        // Parent
        usleep(90000);
        wait(NULL);
    }
}

void load_aliases() {
    // Open the alias file for reading
    FILE *file = fopen(ALIAS_FILE, "r");

    if (file == NULL) {
        perror("Error opening alias file");
        return;
    }
    // Read alias entries from the file
    while (fscanf(file, "alias %s = \"%[^\"]\"", aliases[alias_count].name, aliases[alias_count].command) == 2) {
        alias_count++;
    }

    fclose(file);
}

void save_aliases() {
    // Open the file
    FILE *file = fopen(ALIAS_FILE, "w");
    if (file == NULL) {
        perror("Error opening alias file");
        return;
    }
    // Write alias entries to the file
    for (int i = 0; i < alias_count; i++) {
        fprintf(file, "alias %s = \"%s\"\n", aliases[i].name, aliases[i].command);
    }

    fclose(file);
}

// handling alias with this function
void handle_alias(char *alias_command) {
    char *alias_start = strstr(alias_command, "alias ") + strlen("alias ");
    char *equals_sign = strchr(alias_start, '=');

    if (equals_sign == NULL) {
        printf("Invalid alias syntax\n");
        return;
    }

    *equals_sign = '\0';
    char *alias_name = strtok(alias_start, " ");

    // Find the starting point of the alias command
    char *command_start = equals_sign + 3; // Skip equals sign and spaces
    char *command_end = strrchr(command_start, '"'); // Use strrchr to find the last occurrence of double quote

    if (command_end == NULL) {
        printf("Invalid alias syntax\n");
        return;
    }

    *command_end = '\0'; // Replace the last double quote with null character
    char *command = command_start;

    // Create a new alias
    struct Alias new_alias;
    strcpy(new_alias.name, alias_name);
    strcpy(new_alias.command, command);

    // If an alias with the same name exists, overwrite it
    int existing_alias_index = -1;
    for (int i = 0; i < alias_count; i++) {
        if (strcmp(aliases[i].name, alias_name) == 0) {
            existing_alias_index = i;
            break;
        }
    }

    if (existing_alias_index != -1) {
        aliases[existing_alias_index] = new_alias;
    } else {
        // If it's a new alias, add it to the array
        aliases[alias_count++] = new_alias;
    }

    save_aliases(); // Save the aliases to the file after adding or updating
}
// I replace alias with its corresponding command for example alias egemen = "echo Hello"
// then I write "egemen >> data.txt" then it should be echo Hello >> data.txt
// so this function converts "egemen >> data.txt" to "echo Hello >> data.txt"
char *replace_alias(char *input) {
    // Duplicate the input to avoid modifying the original string
    char *processed_input = strdup(input);

    // Iterate through each alias and replace occurrences in the input
    for (int i = 0; i < alias_count; i++) {
        char *alias_position = strstr(processed_input, aliases[i].name);

        // If the alias is found in the input
        if (alias_position != NULL) {
            // Calculate lengths for alias and command
            int alias_len = strlen(aliases[i].name);
            int command_len = strlen(aliases[i].command);
            int new_len = strlen(processed_input) - alias_len + command_len;

            // Allocate memory for the new input
            char *new_input = (char *) malloc(new_len + 1);

            // Copy the part of the input before the alias
            strncpy(new_input, processed_input, alias_position - processed_input);

            // Copy the alias replacement command
            strcpy(new_input + (alias_position - processed_input), aliases[i].command);

            // Copy the remaining part of the input
            strcpy(new_input + (alias_position - processed_input) + command_len, alias_position + alias_len);

            free(processed_input); // Free the old processed input
            processed_input = new_input; // Update processed input to the new one
            alias_position = strstr(processed_input, aliases[i].name); // Check again in the updated input for additional occurrences
        }
    }
    return processed_input;
}


// this process is for getting total process when we write into terminal "ps -e"
int get_process_count() {
    // fork()
    pid_t pid = fork();

    if (pid == -1) {
        perror("Error forking");
        return -1;
    } else if (pid == 0) {
        // child process
        // Redirect stdout to a file
        freopen("ps.txt", "w", stdout);

        // Execute the "ps -e" command using exec
        execlp("ps", "ps", "-e", (char *)NULL);

        // If exec fails
        perror("Error executing ps");
        _exit(1);
    } else {
        // Parent process
        int status;
        waitpid(pid, &status, 0);

        // Count the number of lines in the file
        FILE *file = fopen("ps.txt", "r");
        if (file == NULL) {
            perror("Error opening ps.txt");
            return -1;
        }

        int process_count = 0;
        char line[256];

        while (fgets(line, sizeof(line), file) != NULL) {
            process_count++;
        }

        fclose(file);
        process_count++; // Incrementation for the parent process

        return process_count;
    }
}

// this is for built-in bello command
void bello(char *input) {
    // Open the bello file for writing
    FILE *bello_file = fopen(BELLO_FILE, "w");
    if (bello_file == NULL) {
        perror("Error opening bello file");
        return;
    }

    // Get user information
    char *username = getlogin();
    char hostname[512];
    gethostname(hostname, sizeof(hostname));
    char *tty = ttyname(STDIN_FILENO);
    char *shell_name = getenv("SHELL");
    char home_location[512];
    strcpy(home_location, getenv("HOME"));

    // Get current time and date
    time_t raw_time;
    struct tm *time_info;
    time(&raw_time);
    time_info = localtime(&raw_time);
    char current_time[80];
    strftime(current_time, 80, "%Y-%m-%d %H:%M:%S", time_info);

    // Get current number of processes being executed
    int process_count = get_process_count();

    // Write information to bello file
    fprintf(bello_file, "Username: %s\n", username);
    fprintf(bello_file, "Hostname: %s\n", hostname);
    fprintf(bello_file, "Last Executed Command: %s\n", LAST_INPUT);
    fprintf(bello_file, "TTY: %s\n", tty);
    fprintf(bello_file, "Current Shell Name: %s\n", shell_name);
    fprintf(bello_file, "Home Location: %s\n", home_location);
    fprintf(bello_file, "Current Time and Date: %s\n", current_time);
    fprintf(bello_file, "Current number of processes being executed: %d\n", process_count);

    // Close the bello file
    fclose(bello_file);

    // Convert "bello" command to "cat bello.txt" and execute
    char cat_command[MAX_INPUT_SIZE];
    strcpy(cat_command, "cat bello.txt ");
    strcat(cat_command, input + 5);
    execute_command(cat_command);
}
