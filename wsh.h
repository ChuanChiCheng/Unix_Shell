#include <stdio.h>
#include <stdlib.h>

typedef struct Job {
    int id;
    pid_t pid;
    char* command;
    struct Job* next;
} Job;

void addJob(pid_t pid, char* command);
void removeJob(int id);
void removeJobByPID(pid_t pid);

void parseInput(char* input, char** args);
int is_background_command(char** args);
void execute_with_pipe(char** args);

void handle_exit(char** args);
void handle_cd(char** args);
void handle_jobs(char** args);
void handle_bg(char** args);
void handle_fg(char** args);

// void sigtstp_handler(int signum);
// void sigint_handler(int signum);
void sigchld_handler(int signum);