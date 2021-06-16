//
//  version 2.0
//
//  shell.c
//  
//
//  Created by Nahom Molla on 31/03/2021.
//
//

#include <fcntl.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <setjmp.h>

/* global variable(s) */
jmp_buf sigint_jump; // added for version 2.0

/* unchanged functions from version 1.0 */
int greet(void); //displays welcome message
char** parse(char** cmd); //parses command to detect redirection
int redirect(char* filename, char* redirect_type); //redirects stdin, stdout, stderr

/* modified functions for version 2.0 */
char** handleCommand(int* newline, int* whitespace, int* pipe_flag); //handles commands from stdin
int checkRedirection(char* str); //checks if redirection operators are used

/* newly added functions for version 2.0 */
void* handlePipe(char* command); //execute pipe'd commands
char** tokenize(char* string, char* delim); //create argv array from a signle string
void signal_handler(int sig); //sigint handler

int main(int argc, const char * argv[]) {
    /* added for version 2.0 *///========================
    struct sigaction handler, ignore;
    
    handler.sa_handler = &signal_handler;
    handler.sa_flags = SA_RESTART;
    sigemptyset(&handler.sa_mask);
    
    ignore.sa_handler = SIG_IGN;
    sigemptyset(&ignore.sa_mask);
    
    //sigint
    sigaction(SIGINT, &handler, NULL);
    
    //sigquit
    sigaction(SIGQUIT, &ignore, NULL);
    //===========================================
    
    int pid, newline, whitespace, pipe_flag;
    char** cmd;
    while (true){
        if (setjmp(sigint_jump) != 0){ //added for version 2.0
            continue;
        }
        
        if (greet() == -1){
            exit(2);
        }
        
        newline = -1;
        whitespace = -1;
        pipe_flag = -1; //added for version 2.0
        cmd = handleCommand(&newline, &whitespace, &pipe_flag);
        if (cmd == NULL){
            if ((newline == 0) || (whitespace == 0) || (pipe_flag == 0)){
                continue;
            }
            else{
                exit(1);
            }
        }

        if (strncmp(cmd[0], "exit", 5) == 0){ //exit shell
            printf("\n[Process Completed]\nshell terminated...\n\n");
            exit(0);
        }
        
        if (strncmp(cmd[0], "cd", 3) == 0){ //change working directory
            if (cmd[1] == NULL){
                chdir("..");
            }
            else{
                chdir(cmd[1]);
            }
            continue;
        }
        
        //non-piping commands
        if ((pid = (fork() == 0))){ //create child process to handle redirection and execution of commands
            cmd = parse(cmd);
            if (cmd == NULL){
                puts("failed to parse");
            }
            if (execvp(cmd[0], cmd) == -1){
                perror("failed to exec");
                exit(1);
            }
            exit(0);
        }
        else if (pid < 0){
            perror("fork failed");
            exit(1);
        }
        else{
            waitpid(pid, NULL, 0);
        }
    }
    return 0;
}

int greet(){ //unmodified from version 1.0
    char* prompt = getenv("PS1");
    if (prompt != NULL){
        printf("%s %% ", prompt);
        return 0;
    }
    char hostname[32], username[32];
    if (gethostname(hostname, 32)){
        perror("failed to get host name");
        return -1;
    }
    if (getlogin_r(username, 32)){
        perror("failed to get login name");
        return -1;
    }

    char cwd[PATH_MAX];
    if (getwd(cwd) == NULL){
        perror("failed to get current working directory");
        return -1;
    }
    
    char* dwd = strdup(cwd);
    char* token = strtok(dwd, "/");
    char foldername[FILENAME_MAX];
    while (token != NULL) {
        snprintf(foldername, strlen(token)+1, "%s", token);
        token = strtok(NULL, "/");
    }
    
    printf("%s@%s %s %% ", username, hostname, foldername);
    return 0;
}

char** handleCommand(int* newline, int* whitespace, int* pipe_flag){ //modified for version 2.0
    char command[ARG_MAX];
    fgets(command, ARG_MAX, stdin);
  
    if (command[0] == '\n'){
        *newline = 0;
        return NULL;
    }
    int len = (int)strlen(command);
    if (command[len-1] == '\n'){
        command[len-1] = '\0';
    }
    
    if (strchr(command, '|') != NULL){ //piping exists
        *pipe_flag = 0;
        return handlePipe(command);
    }
    
    char** res;
    if ((res = tokenize(command, " ")) == NULL){
        *whitespace = 0;
        return NULL;
    }
    return res;
}

void* handlePipe(char* command){ //added for version 2.0
    char* original = strdup(command);
    
    char* left = strtok(original, "|");
    if (left == NULL){
        perror("strtok failed");
    }
    char* right = strtok(NULL, "|");
    if (right == NULL){
        perror("strtok failed");
    }
    
    char** leftCmd=tokenize(left, " ");
    char** rightCmd=tokenize(right, " ");;
    
    int fds[2];
    if (pipe(fds) == -1){
        puts("failed to create pipe");
        exit(10);
    }
    
    int pid1;
    if ((pid1 = fork()) == 0){ //in child process 1
        dup2(fds[1], STDOUT_FILENO); //re-route
        close(fds[0]);
        close(fds[1]);
        execvp(leftCmd[0], leftCmd);
    }
    else if (pid1 < 0){
        perror("fork failed");
        exit(10);
    }
    
    int pid2;
    if ((pid2 = fork()) == 0){ //in child process 2
        dup2(fds[0], STDIN_FILENO); //re-route
        close(fds[1]);
        close(fds[0]);
        execvp(rightCmd[0], rightCmd);
    }
    else if (pid2 < 0){
        perror("fork failed");
        exit(10);
    }
    
    close(fds[0]);
    close(fds[1]);
    waitpid(pid1, NULL, 0);
    waitpid(pid2, NULL, 0);
    return NULL;
}

char** parse(char** cmd){ //unmodified from version 1.0
    int resSize = 0;
    int size=0;
    for (int i=0; cmd[i] != NULL; ++i){
        if (checkRedirection(cmd[i]) == 0){
            --resSize;
            --resSize;
        }
        ++resSize;
        ++size;
    }
    
    // no redirection
    if (resSize == size){
        return cmd;
    }
    
    //redirection
    int res_i=0;
    char** res = malloc((resSize+1) * sizeof(char*));
    if (res == NULL){
        perror("malloc failed");
        return NULL;
    }
    res[resSize] = NULL;
    for (int i=0; cmd[i] != NULL; ++i){
        if (checkRedirection(cmd[i]) == 0){
            if (redirect(cmd[i+1], cmd[i]) == -1){
                puts("failed to redirect");
                return NULL;
            }
            ++i; //skip next elem
        }
        else{
            res[res_i] = malloc((strlen(cmd[i])+1) * sizeof(char));
            if (res[res_i] == NULL){
                perror("malloc failed");
                return NULL;
            }
            res[res_i] = cmd[i];
            ++res_i;
        }
    }
    return res;
}

int redirect(char* filename, char* redirect_type){ //unmodified from version 1.0
    int fd, flag=-1, file_num=-1, file_num_2=-1;
    mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
    if (strncmp(redirect_type, "<", 1) == 0){
        flag = O_RDONLY;
        file_num = STDIN_FILENO;
        file_num_2 = -2;
        mode = 0;
    }
    else if (strncmp(redirect_type, ">>", 2) == 0){
        flag = O_WRONLY | O_CREAT | O_APPEND;
        file_num = STDOUT_FILENO;
        file_num_2 = -2;
    }
    else if (strncmp(redirect_type, ">", 1) == 0){
        flag = O_WRONLY | O_CREAT | O_TRUNC;
        file_num = STDOUT_FILENO;
        file_num_2 = -2;
    }
    else if (strncmp(redirect_type, "2>", 2) == 0){
        flag = O_WRONLY | O_CREAT | O_TRUNC;
        file_num = STDERR_FILENO;
        file_num_2 = -2;
    }
    else if (strncmp(redirect_type, "&>", 2) == 0){
        flag = O_WRONLY | O_CREAT | O_TRUNC;
        file_num = STDOUT_FILENO;
        file_num_2 = STDERR_FILENO;
    }
    
    //error checking
    if (flag == -1 || file_num == -1 || file_num_2 == -1){
        return -1;
    }
    if ((fd = open(filename, flag, mode)) == -1){
        perror("open failed");
        return -1;
    }
    if (dup2(fd, file_num) == -1){
        perror("dup2 failed");
        return -1;
    }
    if ((file_num_2 != -2) && (dup2(fd, file_num_2) == -1)){
        perror("dup2 failed");
        return -1;
    }
    if (close(fd) == -1){
        perror("close failed");
        return -1;
    }
    return 0;
}

char** tokenize(char* string, char* delim){ //added for version 2.0
    char* original = strdup(string);
    char* saved = strdup(original);
    
    int size=0;
    char* token = strtok(original, delim);
    while (token != NULL) {
        ++size;
        token = strtok(NULL, delim);
    }
    
    if (size == 0){
        return NULL;
    }
    
    char** res = malloc((size+1) * sizeof(char*));
    if (res == NULL){
        perror("malloc failed");
        return NULL;
    }
    res[size] = NULL;
    
    token = strtok(saved, delim);
    for (int i=0; i<size; ++i){
        res[i] = malloc((strlen(token)+1) * sizeof(char));
        if (res[i] == NULL){
            perror("malloc failed");
            return NULL;
        }
        res[i] = token;
        token = strtok(NULL, " ");
    }
    return res;
}

int checkRedirection(char* str){ //modified for version 2.0
    if ((strncmp(str, "<", 1) == 0) || (strncmp(str, "2>", 2) == 0) ||
        (strncmp(str, ">>", 2) == 0) || (strncmp(str, "&>", 2) == 0) ||
        (strncmp(str, ">", 1) == 0))
        return 0;
    return -1;
}

void signal_handler(int sig){ //added for version 2.0
    if (sig == SIGINT){
        fflush(stdout);
        write(STDOUT_FILENO, "\n", 1);
        longjmp(sigint_jump, 1);
    }
}
