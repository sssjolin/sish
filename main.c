#include <sys/signalfd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "util.h"

#define EXEC_FAIL 127
#define READ  0
#define WRITE 1


extern char **environ;
int status;
static int track = 0;
int file_in_num = STDIN_FILENO;
int file_out_num = STDOUT_FILENO;
char* cmds[512];
int background = 0;
pid_t pgid;
int f_pgid = 0;


static void
usage(void)
{
    fprintf(stderr, "Usage: sish [ −x] [ −c command]\n");
    exit(EXIT_FAILURE);
}



char*
skipspace(char* s){
    while (isspace(*s))
        s++;
    return s;
}

int
handlePipe(int i,int input){
    if (cmds[1] == NULL)
        return file_in_num;

    int pipettes[2];
    pipe(pipettes);

    if (i == 0){
        file_out_num = pipettes[WRITE];
    }
    else if (i != 0 && cmds[i + 1] != NULL&&input != 0){
            file_in_num = input;
            file_out_num = pipettes[WRITE];
    }
    else{
            file_in_num = input;
    }
    return pipettes[READ];
}


bool
redirect(char* cmd){
    int in_fd, out_fd,flag;

    char* tmp;
    char file[512];
    char* gt = strchr(cmd, '>');

    while (gt != NULL){
		flag = O_CREAT | O_RDWR | O_TRUNC;
        *gt++ = ' ';
        if (*gt == '>'){
			flag = O_CREAT | O_RDWR | O_APPEND;
            *gt++ = ' ';
        }
        gt = skipspace(gt);
        
        if (*gt == '\0'|| *gt == '>' || *gt == '<'){
            fprintf(stderr, "sish: syntax error near unexpected token '>'");
            if (*gt != '\0')
                *gt = ' ';
            return false;
        }
        tmp = strchr(gt, ' ');
        if (tmp != NULL){
            strncpy(file, gt, tmp - gt);
            file[tmp - gt] = 0;
            memset(gt, ' ', tmp - gt);
        }
        else{
            strncpy(file, gt, sizeof(gt));
            file[sizeof(gt)] = 0;
            *gt = 0;
        }
        out_fd = open(file, flag, 0666);
        if (file_out_num != STDOUT_FILENO)
            close(file_out_num);
        file_out_num = out_fd;
        if (tmp != NULL)
            gt = strchr(tmp, '>');
        else
            break;
    }

    char* lt = strchr(cmd, '<');
    while (lt != NULL){
        flag = O_CREAT | O_RDONLY;
        *lt++ = ' ';
        lt = skipspace(lt);
        if (*lt == '\0' || *lt == '>'||*lt == '<'){
            fprintf(stderr, "sish: syntax error near unexpected token '<'");
            if (*lt != '\0')
                *lt = ' ';
            return false;
        }
        tmp = strchr(lt, ' ');
        if (tmp != NULL){
            strncpy(file, lt, tmp - lt);
            file[tmp - lt] = 0;
            memset(lt, ' ', tmp - lt);
        }
        else{
            strncpy(file, lt, sizeof(lt));
            file[sizeof(lt)] = 0;
            *lt = 0;
        }
        in_fd = open(file, flag, 0666);
        if (file_in_num != STDIN_FILENO)
            close(file_in_num);
        file_in_num = in_fd;
        if (tmp != NULL)
            lt = strchr(tmp, '<');
        else
            break;
    }
    return true;
}


void
execute(char* args[]){
    pid_t pid;
    int status;

	pid = fork();

	if (background == 1 && f_pgid == 0){
		pgid = pid;
		f_pgid = 1;
	}


    if (pid < 0){
        fprintf(stderr, "sish: can't fork: %s\n",
            strerror(errno));
    }
    else if (pid == 0){

		setpgid(0, pgid);

        dup2(file_out_num, STDOUT_FILENO);
        dup2(file_in_num, STDIN_FILENO);
        if ((status = execvp(args[0], args)) < 0){
            
            fprintf(stderr, "sish: couldn't exec %s: %s\n", args[0],
                strerror(errno));
            exit(EXEC_FAIL);
        }
    }
    if (file_out_num != STDOUT_FILENO)
        close(file_out_num);
    if (file_in_num != STDIN_FILENO)
        close(file_in_num);

    if (background == 0){
        if ((pid = waitpid(pid, &status, 0)) < 0)
            fprintf(stderr, "sish: waitpid error: %s\n",
            strerror(errno));
    }
    file_out_num = STDOUT_FILENO;
    file_in_num = STDIN_FILENO;

    return;
}




void
split(char* cmd, char* args[])
{
    cmd = skipspace(cmd);
    if (*cmd == '\0')
        return;
    if (!redirect(cmd))
        return;
    cmd = skipspace(cmd);
    char* next = strchr(cmd, ' ');
    
    int i = 0;


    while (next != NULL){
        
        *next = 0;

        args[i++] = cmd;

        cmd = skipspace(next + 1);
        next = strchr(cmd, ' ');
    }
    
    if (*cmd != 0){
        args[i++] = cmd;
    }

    args[i] = 0;
    
    if (track == 1){
        i = 0;
        printf("+");
        while (args[i] != 0)
            printf(" %s", args[i++]);
        printf("\n");
    }


    return;
}




void
splitCmds(char* line)
{
    line = skipspace(line);
    if (*line == '\0'){
        cmds[0] = NULL;
        return;
    }

    char* bg = strrchr(line, '&');
    if ((bg != NULL)&&(*(bg+1)==0)){
        background = 1;
        *bg = 0;
    }


    char* slash = strchr(line, '|');
    int i = 0;
    while (slash != NULL){
        if ((slash - line) == 0){
            warnx("syntax error near unexpected token '|'\n");
            cmds[0] = NULL;
            return;
        }
        else{
            *slash = '\0';
            cmds[i++] = line;
            line = ++slash;
            slash = strchr(line, '|');
        }
    }
    if (*line != 0)
        cmds[i++] = line;
    cmds[i] = 0;
    return;
}


int
main(int argc, char* argv[])
{
    if (signal(SIGINT, SIG_IGN) != SIG_IGN)
        signal(SIGINT, ignore);


    char buf[1024];
    char ch;
    int single_cmd = 0;
    char* args[512];
    int input = 0;
    char cwd[512];
    
    while ((ch = getopt(argc, argv, "xc:")) != -1){
        switch (ch){
        case 'x':
            track = 1;
            break;
        case 'c':
            single_cmd = 1;
            strcpy(buf, optarg);
            while (optind < argc){
                strcat(buf, " ");
                strcat(buf, argv[optind++]);
            }

            break;

        }
    }

    if (getcwd(cwd, sizeof(cwd)) != NULL){
        if (cwd[sizeof(cwd) - 1] != '/')
            strcat(cwd, "/");
        strcat(cwd, argv[0]);
        setenv("SHELL", cwd, 1);
    }



    if (single_cmd == 1){
        splitCmds(buf);
        int i = 0;
        while (cmds[i] != NULL){
            input = handlePipe(i, input);
            split(cmds[i], args);
            if (check_buildin(args) == 0)
                execute(args);
            i++;
        }
        return status;

    }

    if (optind != argc)
        usage();

    while (1){

        fgets(buf, sizeof(buf), stdin);
        buf[strlen(buf) - 1] = '\0';

        splitCmds(buf);
        int i = 0;
        while (cmds[i] != NULL){
            input=handlePipe(i,input);
            split(cmds[i], args);
            if (args[0] != NULL){
                if (check_buildin(args) == 0)
                    execute(args);
            }
            i++;
        }

		background = 0;
		f_pgid = 0;


    }
    return 0;

}
