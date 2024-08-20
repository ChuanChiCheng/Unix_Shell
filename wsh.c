#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "wsh.h"

#define MAX_ARG_SIZE 100

Job* jobs = NULL;
int nextJobId = 1;
pid_t foreground_pid = -1;


// Handle SIGCHLD
// void sigchld_handler(int signum) {
//     // printf("Received SIGCHLD\n");
//     int status;
//     pid_t child_pid;
//     while ((child_pid = waitpid(-1, &status, WNOHANG | WUNTRACED)) > 0) {
//         if (WIFEXITED(status)) {
//             // printf("Child %d exited with status %d\n", child_pid, WEXITSTATUS(status));
//             //removeJobByPID(child_pid);
//         } else if (WIFSIGNALED(status)) {
//             // printf("Child %d was terminated by signal %d\n", child_pid, WTERMSIG(status));
//             //removeJobByPID(child_pid);
//         } else if (WIFSTOPPED(status)) {
//             // printf("Child %d was stopped by signal %d\n", child_pid, WSTOPSIG(status));
//         } else if (WIFCONTINUED(status)) {
//             // printf("Child %d continued\n", child_pid);
//         }
//     }
// }


void handle_exit(char** args) {
    exit(0);
}

void handle_cd(char** args) {
    if (args[1] == NULL) {
        fprintf(stderr, "cd: missing argument\n");
    } else {
        if (chdir(args[1]) != 0) {
            perror("cd");
        }
    }
}

void handle_jobs(char** args) {
    // Print jobs
    Job* current = jobs;
    while (current != NULL) {
        printf("%d: %s\n", current->id, current->command);
        current = current->next;
    }
}

void handle_fg(char** args) {
    int jobId;
    if (args[1] == NULL) {
        // If no argument is provided, use the largest job <id>
        jobId = nextJobId - 1;
    } else {
        jobId = atoi(args[1]);
    }

    Job* current = jobs;
    pid_t s_pid = getpid();
    while (current != NULL) {
        if (current->id == jobId) {
            pid_t gpid = getpgid(current->pid);
            kill(-gpid, SIGCONT);  // Continue the job first
            
            int ret = 0, status;
            tcsetpgrp(STDIN_FILENO, gpid); // Now set the job to the foreground
            if (waitpid(current->pid, &status, WUNTRACED) > 0 && !WIFSTOPPED(status))
                ret = 1;
            
            tcsetpgrp(STDIN_FILENO, s_pid);  // Set the shell back to the foreground
            if (ret > 0) removeJob(jobId);
            return;
        }
        current = current->next;
    }
    fprintf(stderr, "fg: job not found\n");
}

void handle_bg(char** args) {
    int jobId;
    if (args[1] == NULL) {
        fprintf(stderr, "bg: job id required\n");
        return;
    }
    jobId = atoi(args[1]);

    Job* current = jobs;
    while (current != NULL) {
        if (current->id == jobId) {
            kill(current->pid, SIGCONT); // Continue the job in the background
            current = current->next;
            return;
        }
    }
    fprintf(stderr, "bg: job not found\n");
}


int is_background_command(char** args) {
    int i = 0;
    while (args[i] != NULL) {
        i++;
    }
    if (i > 0 && strcmp(args[i - 1], "&") == 0) {
        args[i - 1] = NULL;
        return 1;
    }
    return 0;
}

void addJob(pid_t pid, char* command) {
    // Add a new job to the head of list
    Job* newJob = malloc(sizeof(Job));
    newJob->id = nextJobId++;
    newJob->pid = pid;

    newJob->command = strdup(command);
    newJob->next = NULL;

    if(jobs == NULL){
        jobs = newJob;
        
    } else {
        Job* current = jobs;
        while(current->next != NULL){
            current = current->next;
        }
        current->next = newJob;
    }
}

void removeJob(int id) {
    // Remove a job from the list by its ID
    Job* current = jobs;
    Job* prev = NULL;
    while (current != NULL) {
        if (current->id == id) {
            if (prev != NULL) {
                prev->next = current->next;
            } else {
                jobs = current->next;
            }
            free(current->command);
            free(current);
            return;
        }
        prev = current;
        current = current->next;
    }
    printf("Removed job with ID: %d\n", id);
}

// void removeJobByPID(pid_t pid) {
//     // Remove a job from the list by its PID
//     Job* current = jobs;
//     Job* prev = NULL;
//     while (current != NULL) {
//         if (current->pid == pid) {  // Use the `pid` attribute to compare
//             if (prev != NULL) {
//                 prev->next = current->next;
//             } else {
//                 jobs = current->next;
//             }
//             free(current->command);
//             free(current);
//             return;
//         }
//         prev = current;
//         current = current->next;
//     }
// }

void parseInput(char* input, char** args) {
    int i = 0;
    char* token = strtok(input, " \n");
    while (token != NULL) {
        args[i] = token;
        i++;
        token = strtok(NULL, " \n");
    }
    args[i] = NULL;
}


void execute_with_pipe(char** args) {
    char* commands[100][100]; 
    int num_commands = 1;
    int pipes[100][2];

    int index = 0;
    int argv = 0;
    while (args[index] != NULL) {
        if (strcmp(args[index], "|") == 0) {
            commands[num_commands - 1][argv] = NULL;
            num_commands++;
            argv = 0;
        } else {
            commands[num_commands - 1][argv] = args[index];
            argv++;
        }
        index++;
    }
    commands[num_commands - 1][argv] = NULL;

    // Create pipes
    for (int i = 0; i < num_commands; i++) {
        if (pipe(pipes[i]) == -1) {
            perror("pipe");
            return;
        }
    }

    // Execute commands
    for (int i = 0; i < num_commands; i++) {
        if (fork() == 0) {
            // If not the first command, redirect standard input from the previous pipe
            if (i != 0) {
                dup2(pipes[i - 1][0], STDIN_FILENO);
            }
            // If not the last command, redirect standard output to the next pipe
            if (i != num_commands - 1) {
                dup2(pipes[i][1], STDOUT_FILENO);
            }

            // Close all pipes in child
            for (int j = 0; j < num_commands - 1; j++) {
            close(pipes[j][0]);
            close(pipes[j][1]);
        }
            // Execute the command
            execvp(commands[i][0], commands[i]);
            perror("execvp");
            exit(1);
        }
    }

    // Parent process
    // Close all pipes in the parent
    for (int i = 0; i < num_commands - 1; i++) {
        close(pipes[i][0]);
        close(pipes[i][1]);
    }

    // Wait for all child processes to finish
    for (int i = 0; i < num_commands; i++) {
        wait(NULL);
    }
}

pid_t shell_pid;
pid_t shell_pgid;

int main() {
    char* input = NULL;
    size_t len = 0;
    ssize_t read;
    char* args[MAX_ARG_SIZE];
    shell_pid = getpid();
    shell_pgid = getpgid(shell_pid); 
    char originalInput[MAX_ARG_SIZE];
    

    // Set up signal handlers
    // signal(SIGCHLD, sigchld_handler);
    signal(SIGTSTP, SIG_IGN);
    signal(SIGINT, SIG_IGN);
    signal(SIGTTOU, SIG_IGN);

    while (1) {
        printf("wsh> ");
	    fflush(stdout);
        read = getline(&input, &len, stdin);
        if (read == -1) { // EOF received (generate by Ctrl-D)
            exit(0);
        }
        strcpy(originalInput, input);
        parseInput(input, args);

        size_t len = strlen(originalInput);
        if (len > 0 && originalInput[len-1] == '\n') {
            originalInput[len-1] = '\0';
        }

        int is_background_cmd = is_background_command(args);
        if (args[0] == NULL) {
            continue;
        }

        // ----- [start] built-in command -----
        if (strcmp(args[0], "exit") == 0) {
            if (args[1] != NULL) {
                fprintf(stderr, "exit: too many arguments\n");
                continue;
            }
            handle_exit(args);
        } else if (strcmp(args[0], "cd") == 0) {
            if (args[1] == NULL || args[2] != NULL) {
                fprintf(stderr, "cd: invalid number of arguments\n");
                continue;
            }
            handle_cd(args);
            continue;
        }  else if (strcmp(args[0], "jobs") == 0) {
            handle_jobs(args);
            continue;
        } else if (strcmp(args[0], "fg") == 0) {
            handle_fg(args);
            continue;
        } else if (strcmp(args[0], "bg") == 0) {
            handle_bg(args);
            continue;
        }
        // ----- [end] built-in command -----
        int found_pipe = 0;
        for (int i = 0; args[i] != NULL; i++) {
            if (strcmp(args[i], "|") == 0) {
                execute_with_pipe(args);
                found_pipe = 1;
                break;
            }
        }
        if(found_pipe){
            continue;
        }

        pid_t pid = fork();

        if (pid < 0) {
            perror("Failed to fork");
            exit(1);
        } else if (pid == 0) {
            // Child process
            signal(SIGTSTP, SIG_DFL);
            signal(SIGINT, SIG_DFL);
            signal(SIGQUIT, SIG_DFL);

            if (execvp(args[0], args) < 0) {
                perror("Failed to execute command");
                exit(1);
            }
        } else {
            // Parent process
            signal(SIGTTOU, SIG_IGN);
            
            if (is_background_cmd == 1) {
                addJob(pid, originalInput); // Add the background job to the jobs list
                continue; // No need to wait background command
            }
            foreground_pid = pid;
            setpgid(pid, pid);
            tcsetpgrp(STDIN_FILENO, pid);
            int status;
            waitpid(pid, &status, WUNTRACED);    
            
            if (WIFSTOPPED(status)) {
                // The child was stopped by a signal (likely SIGTSTP)
                addJob(pid, originalInput);
            }
            foreground_pid = -1;
            tcsetpgrp(STDIN_FILENO, shell_pgid);
        }
    }

    free(input);
    return 0;
}
