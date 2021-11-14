#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/types.h>
#include <errno.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>

int prepare(void);
int process_arglist(int count, char **arglist);
int finalize(void);
int doBackground(int count, char **arglist);
int doRedirection(int count, char **arglist);
int doPipe(int count, char **arglist, int whereIsSym);
int doRegular(int count, char **arglist);
int process_arglist(int count, char **arglist);
void which_command(int count, char **arglist, int* res);
void SIGINT_handler(int shouldTerminate);

int prepare(void){
    struct sigaction shell;
    shell.sa_handler = SIG_IGN;
    if(sigaction(SIGINT, &shell ,NULL)== -1){
        perror("failed init shell");
        exit(1);
    }
    return 0;
}

int doBackground(int count, char **arglist){
    char* cmd;
    int pid;

    cmd = arglist[0];
    pid = fork();
    if(pid < 0){
        perror("failed fork");
        return 0;
    }
    // Child
    if(pid == 0){
       SIGINT_handler(0);
        arglist[count-1] = NULL;
        execvp(cmd, arglist);
        // will only reach this line if execvp fails
        perror("failed execvp");
        exit(1);
    }
    // Parent doesn't have to wait for child to return
    return 1;
}

int doRedirection(int count, char **arglist){
    char *cmd, *to;
    int fileDesc, pid;

    cmd = arglist[0];
    arglist[count-2] = NULL;
    to = arglist[count-1];
    fileDesc = open(to, O_RDWR | O_CREAT, 00700);
    if(fileDesc < 0){
        perror("failed to open file");
        return 0;
    }
    pid = fork();
    if(pid < 0){
        perror("failed fork");
        return 0;
    }
    // Child
    if(pid == 0){
       SIGINT_handler(1);
        if(dup2(fileDesc,1) == -1){
            close(fileDesc);
            perror("failed dup2");
            exit(1);
        }
        if(close(fileDesc)== -1){
            perror("failed to close fileDesc");
            exit(1);
        }
        execvp(cmd, arglist);
        // will only reach this line if execvp fails
        perror("failed execvp");
        exit(1);
    }
    // Parent
    close(fileDesc);
    // make parent wait until child process is done - no zombies!
    wait(NULL);
    return 1;
}

int doPipe(int count, char **arglist, int whereIsSym){
    int p, pid[2], fd[2], r, w;
    char *cmd;

    cmd = arglist[whereIsSym+1];
    arglist[whereIsSym] = NULL;
    p = pipe(fd);
    if(p == -1){
        perror("failed pipe");
        return 0;
    }
    r = fd[0];
    w = fd[1];
    pid[0] = fork();
    // Child 1 - writes to stdout
    if(pid[0] < 0){
        perror("failed fork");
        return 0;
    }
    //child 1
    if(pid[0] == 0){
        SIGINT_handler(1);
         if(close(w) == -1){
            perror("failed to close write");
            exit(1);
        }
        if(dup2(r,0) == -1){
            perror("failed dup2");
            exit(1);
        }
        if(close(r) == -1){
            perror("failed to close read");
            exit(1);
        }
        execvp((arglist + whereIsSym + 1)[0], arglist + whereIsSym + 1);
        // will only reach this line if execvp fails
        perror("failed execvp");
        exit(1);
    }
    // back to parent
    pid[1] = fork();
    // child 2  - accecpts input from stdin
    if(pid[1] < 0){
        perror("failed fork");
        return 0;
    }
    cmd = arglist[0];
    // child 2
    if(pid[1] == 0){
        SIGINT_handler(1);
        if(close(r) == -1){
            perror("failed to close read");
            exit(1);
        }
        if(dup2(w,1) == -1){
            perror("failed dup2");
            exit(1);
        }
        if(close(w) == -1){
            perror("failed to close write");
            exit(1);
        }
        execvp(cmd, arglist);
    }
    // make parent wait until  all child process is done - no zombies!
    if(close(r) == -1){
        perror("failed to close read");
        return 0;
    }
    if(close(w) == -1){
        perror("failed to close write");
        return 0;
    } 
    wait(NULL);
    return 1;
}

int doRegular(int count, char **arglist){
    char *cmd;
    int pid;

    cmd = arglist[0];
    pid = fork();
    if(pid < 0){
        perror("failed fork");
        return 0;
    }
    // Child
    if(pid == 0){
        SIGINT_handler(1);
        execvp(cmd, arglist);
        // will only reach this line if execvp fails
        perror("failed execvp");
        exit(1);
    }
    // Parent
    // make parent wait until child process is done - no zombies!
    wait(NULL);
    return 1;
}

int process_arglist(int count, char **arglist){
    int *whichCmdAndWhere;
    
    whichCmdAndWhere = (int*)calloc(2, sizeof(int));
    which_command(count, arglist, whichCmdAndWhere);
    /**
     * &
     **/
    if(whichCmdAndWhere[0] == 1){
       return doBackground(count, arglist);
    }
     /**
     * >
     **/
    if(whichCmdAndWhere[0] == 2){
       return doRedirection(count, arglist);
    }
     /**
     * |
     **/
    if(whichCmdAndWhere[0] == 3){
        return doPipe(count, arglist, whichCmdAndWhere[1]);
    }
     /**
     * regular command
     **/
    if(whichCmdAndWhere[0] == 4){
        return doRegular(count, arglist);
    }
    return 1;  
}

int finalize(void){
    return 0;
}

/**
 * In order to understand what is the command type, we will check for the special characters in the locations described at "ASSUMPTIONS" section:
 *
 * & - the last word of the command line, will appear at arglist[count -2]. The function will return command type = 1.
 * > - appears one before last on the command line, will appear at arglist[count -3]. The function will return command type = 2. 
 * | - appears somewhere in the command, so we will scan it. The function will return command type = 3. 
 * regular command - a command with no special character. The function will return command type = 4. 
 * 
 **/
void which_command(int count, char **arglist, int* res){
    int i;

    if(count >=2){
        if (strcmp(arglist[count -1],"&") == 0){
        res[0] = 1;
        res[1] = count -1;
        return;
        }
        if (strcmp(arglist[count -2], ">") == 0){
            res[0] = 2;
            res[1] = count -2;
            return;
        }
        for(i = 0; i < count; i++){
            if(strcmp(arglist[i], "|") == 0){
                res[0] = 3;
                res[1] = i;
                return;
            }
        }
        res[0] = 4;
        res[1] = 0;
        return;
    }
    else{
        res[0] = 4;
        res[1] = 0;
        return; 
    }
}

void SIGINT_handler(int shouldTerminate){
    struct sigaction sig;
    int signal, changed;
    sig.sa_flags = SA_RESTART;
    
    if(shouldTerminate == 1){
        sig.sa_handler = SIG_DFL;
        signal = SIGINT;
        changed = sigaction(signal, &sig, NULL);
        if(changed == -1){
            perror("failed signals");
        }
    }
     if(shouldTerminate == 0){
        sig.sa_handler = SIG_IGN;
        signal = SIGCHLD;
        changed = sigaction(signal, &sig, NULL);
        if(changed == -1){
            perror("failed signals");
        }
    }
}