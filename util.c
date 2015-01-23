#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <pwd.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "util.h"

void
change_dir(char* args[]){
    char* home_dir;
    char* username;
    if (args[1] == NULL){
        if ((home_dir = getenv("HOME")) != NULL){
            if (chdir(home_dir) < 0){
                fprintf(stderr, "Can't change directory into home directory\n");
            }
            else if ((username = getlogin()) != NULL){
                struct passwd *pw;
                if ((pw = getpwnam(username)) == NULL){
                    fprintf(stderr, "Can't change directory into home directory\n");
                    return;
                }
                if ((chdir(pw->pw_dir)) < 0){
                    fprintf(stderr, "Can't change directory into home directory\n");
                    return;
                }
            }
            else{
                fprintf(stderr, "Can't change directory into home directory\n");
                return;
            }
        }
    }
    else if (args[2] != NULL)
        fprintf(stderr, "Usage: cd [dir]\n");
    else{
        if (chdir(args[1]) < 0)
            fprintf(stderr, "Can't change directory into %s\n", args[1]);
    }
    return;
}

void
echo(char* args[]){
    if (args[1] == NULL)
        fprintf(stderr, "Usage: echo [word]\n");
    else{
        if (strcmp(args[1], "$?") == 0)
            dprintf(file_out_num, "%d\n", status);
        else if (strcmp(args[1], "$$") == 0){
            dprintf(file_out_num, "%d\n", getpid());
        }
        else if(*args[1]=='$') {
            char* variable;
            if ((variable = getenv(args[1]+1)) != NULL)
                dprintf(file_out_num, "%s\n", variable);
            else
                dprintf(file_out_num, "\n");
        }
        else{
            int i = 1;
            while (args[i]!=NULL)
                dprintf(file_out_num, " %s", args[i++]);
            dprintf(file_out_num, "\n");
        }          
    }
    if (file_out_num != STDOUT_FILENO)
        close(file_out_num);
}

int
check_buildin(char* args[]){
    if (strcmp(args[0], "exit") == 0) {
        exit(0);
    }
    else if (strcmp(args[0], "cd") == 0) {
        change_dir(args);
        return 1;
    }
    else if (strcmp(args[0], "echo") == 0) {
        echo(args);
        return 1;
    }

    return 0;
}


void
ignore(int sig)
{
    fprintf(stderr, "\n"); 
}