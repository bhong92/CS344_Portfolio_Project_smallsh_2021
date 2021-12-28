#define _GNU_SOURCE
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/wait.h> 
#include <stdbool.h>


/*Global Variables*/
int status = 0;
int background[100];
int bgCount;
bool flgBG = false;
bool flgTSTP = false;
pid_t spawnPid = -5;
int childStatus = -5;
int choice = 0;

/*structure to hold input info*/
struct commands {
    char* outPath; 
    char* inPath;
    char* cmd;
    char* arg;
    int c;
};


/*Handler function to process ^C SIGINT Signal*/
void handle_SIGINT(int sig) {
    char* message = "terminated by signal 2\n";
    write(1, message, 24);
    //printf("terminated by signal %d\n", sig);
    fflush(stdout);
}

/*Handler to process ^Z SIGTSTP signal*/
void handle_SIGTSTP() {
    //if not in foreground-only mode - enter mode
    if (!flgTSTP) {
        flgBG = false;
        flgTSTP = true;
        char* message = "Entering foreground-only mode (& is now ignored)\n";
        write(1, message, 50);
        fflush(stdout);
    }
    else {
        //if in foreground only mode - exit mode
        flgTSTP = false;
        char* message = "Exiting foreground-only mode\n";
        write(1, message, 30);
        fflush(stdout);
    }
}

/*checks for the background processes*/
void bgProcess() {
    for (int i = 0; i <= bgCount; i++) {
        if (waitpid(background[i], &childStatus, WNOHANG) > 0) {
            if (WIFSIGNALED(childStatus)) {
                printf("background pid %d is done: ", background[i]);
                printf("terminated by signal %d\n", WTERMSIG(childStatus));
                fflush(stdout);
            }
            if (WIFEXITED(childStatus)) {
                printf("background pid %d is done: exit value %d\n", background[i], childStatus);
            }
        }
    }
}

/*Checks the input for $$ to expand the PID*/
void expand(char arr1[2048]) {
    char* ret = strstr(arr1, "$$");
    while (ret) {
        char str[2048];
        sprintf(str, "%d", getpid());
        size_t len1 = strlen("$$");
        size_t len2 = strlen(str);
        if (len1 != len2)
        {
            memmove(ret + len2, ret + len1, strlen(ret + len1) + 1);
        }
        memmove(ret, str, len2);
        ret = strstr(arr1, "$$");
    }
}

/*receives input from user and processes*/
struct commands* get_input(struct commands* cmd) {
    //catch signals
    signal(SIGTSTP, handle_SIGTSTP);
    signal(SIGINT, handle_SIGINT);

    //struct commands* cmd;
    cmd = (struct commands*)malloc(sizeof(struct commands));

    //to receive input
    char cmdLine[2048];

    fgets(cmdLine, 2048, stdin);

    //remove the \n from input
    char* input = strtok(cmdLine, "\n");

    //if input was blank return and do nothing
    if (strlen(cmdLine) == 1) {
        cmd->c = 0;
        return cmd;
    }

    //calculations for the first and last inputs
    int l = strlen(input) - 1;
    char first_char[2] = { input[0] };
    char last_char[2] = { input[l] };

    //if first character was '#' then return and do nothing
    if (!strcmp(first_char, "#") || !strlen(first_char)) {
        cmd->arg = (char*)calloc(strlen(input) + 1, sizeof(char));
        strcpy(cmd->arg, input);
        cmd->c = 0;
        return cmd;
    }

    //if input contains $$ expand variable
    if (strstr(input, "$$")) {
        expand(input);
    }

    //check that & is in the input
    //check the "&" is at the end of the input
    //set background flag flgBG to true
    if (strstr(input, " &")) {
        if (!strcmp(last_char, "&")) {
            if (!flgTSTP) { flgBG = true; }
            strtok(input, "&");
        }
    }

    //store input into cmd->arg
    cmd->arg = (char*)calloc(strlen(input) + 1, sizeof(char));
    strcpy(cmd->arg, input);

    //delimit input to check for built in commands
    char* token = strtok(input, " \n");
    //check CD before moving with other commands
    if (!strcmp(token, "cd")) {
        cmd->c = 2;
        token = strtok(NULL, " \n");
        if (token) {
            cmd->cmd = (char*)calloc(strlen(token) + 1, sizeof(char));
            strcpy(cmd->cmd, token);
        }
        else {
            cmd->cmd = (char*)calloc(strlen(getenv("HOME")) + 1, sizeof(char));
            strcpy(cmd->cmd, getenv("HOME"));
        }
        return cmd;
    }
    //built in command exit
    if (!strcmp("exit", token)) {
        cmd->c = 1;
        return cmd;
    }

    //built in command status
    if (!strcmp("status", token)) {
        cmd->c = 3;
        return cmd;
    }

    cmd->c = 4;
    return cmd;
}
/*execution of inputs*/
int execute(struct commands* cmd) {
    signal(SIGTSTP, handle_SIGTSTP);

    char* arg[2048];
    int m = 0;
    int fd;
    int fd2;
    char* tempFile;
    bool redirect = false;

    arg[m] = strtok(cmd->arg, " \n");
    while (arg[m] != NULL) {
        m++;
        arg[m] = strtok(NULL, " \n");
        if (!arg[m]) { break; }

        //check for outpath
        if (!strcmp(arg[m], ">")) {
            //file path gets saved into array and cmd->outpath
            arg[m] = strtok(NULL, " \n");
            cmd->outPath = (char*)calloc(strlen(arg[m]) + 1, sizeof(char));
            strcpy(cmd->outPath, arg[m]);
            m--;
        }

        //check for in path
        if (!strcmp(arg[m], "<")) {
            //file path gets saved into array and cmd->inpath
            arg[m] = strtok(NULL, " \n");
            cmd->inPath = (char*)calloc(strlen(arg[m]) + 1, sizeof(char));
            strcpy(cmd->inPath, arg[m]);
            m--;
        }
    }

    if (cmd->inPath)
    {
        tempFile = strtok(cmd->inPath, " \n");
        fd = open(tempFile, O_RDONLY, 0);
        if (fd < 0)
        {
            printf("cannot open %s for input\n", tempFile);
            fflush(stdout);
            exit(1);
        }
        else {
            dup2(fd, 0);
            redirect = true;
        }
    }

    if (cmd->outPath)
    {
        tempFile = strtok(cmd->outPath, " \n");
        fd2 = open(tempFile, O_CREAT | O_WRONLY | O_TRUNC, 0666);
        if (fd2 < 0)
        {
            printf("cannot open %s for output\n", tempFile);
            fflush(stdout);
            exit(1);
        }
        else
        {
            dup2(fd2, 1);
            redirect = true;
        }
    }

    if (redirect) {
        status = execvp(arg[0], arg);
        if (fd) {
            close(fd);
        }
        if (fd2) {
            close(fd2);
        }
        redirect = false;
    }


    if ((status = execvp(arg[0], arg)) != 0)
    {
        printf("%s: no such file or directory found\n", arg[0]);
    }

    return status;
}

/*fork processes to run execution*/
void forkIt(struct commands* cmd) {
    spawnPid = fork();
    switch (spawnPid) {
    case -1:
        perror("fork() failed!");
        fflush(stdout);
        status = -1;
        break;

    case 0:
        //child process with signal SIGINT default
        signal(SIGINT, SIG_DFL);
        status = execute(cmd);
        fflush(stdout);
        cmd->c = 0;
        break;

    default:
        //parent process with signal handler
        //signal(SIGINT, handle_SIGINT);
        if (flgBG) {
            printf("background pid is %d\n", spawnPid);
            background[bgCount] = spawnPid;
            bgCount++;
            waitpid(spawnPid, &childStatus, WNOHANG);
            flgBG = false;
            fflush(stdout);
            choice = 0;
            break;
        }
        else {
            waitpid(spawnPid, &childStatus, 0);
            if (WIFEXITED(childStatus)) {
                status = WEXITSTATUS(childStatus);
            }
            cmd->c = 0;
            break;
        }
    }
}

/*change directories*/
void cd(struct commands* cmd) {
    chdir(cmd->cmd);
    printf("%s\n", getcwd(cmd->cmd, 100));
    fflush(stdout);
}

int main() {
    struct sigaction SIGINT_action = { 0 };
    struct sigaction SIGTSTP_action = { 0 };
    SIGINT_action.sa_handler = handle_SIGINT;
    SIGTSTP_action.sa_handler = handle_SIGTSTP;
    sigfillset(&SIGINT_action.sa_mask);
    sigfillset(&SIGTSTP_action.sa_mask);
    sigaction(SIGINT, &SIGINT_action, NULL);
    sigaction(SIGTSTP, &SIGTSTP_action, NULL);
    SIGINT_action.sa_flags = 0;
    SIGTSTP_action.sa_flags = 0;

    int choice;

    struct commands* cmd;

    while (choice != 1) {

        bgProcess();
        cmd = get_input(cmd);
        choice = cmd->c;

        if (choice == 1) {
            //exit smallsh
            break;
        }
        else if (choice == 2) {
            //change directories
            cd(cmd);
        }
        else if (choice == 3) {
            //return status
            printf("exit value %d\n", status);
        }
        else if (choice == 4) {
            //run input
            forkIt(cmd);
        }
    }
}