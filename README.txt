README:
smallsh to emulate a subset of features of well-known shells such as bash


    Provides a prompt for running commands
    Handles blank lines and comments, which are lines beginning with the # character
    Provides expansion for the variable $$
    Executes 3 commands exit, cd, and status via code built into the shell
    Executes other commands by creating new processes using a function from the exec family of functions
    Supports input and output redirection
    Supports running commands in foreground and background processes
    Implements custom handlers for 2 signals, SIGINT and SIGTSTP


To run program please make sure file is in this format

The following command line is used to run the smallsh.c
gcc -std=gnu99 -o smallsh smallsh.c
./smallsh