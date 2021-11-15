#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/types.h>
#include <errno.h>


int prepare(void);
int process_arglist(int count, char **arglist);
int finalize(void);
int Background(int count, char **arglist);
int Redirection(int count, char **arglist);
int Pipe(char **arglist, int whereIsSym);
int Regular(int count, char **arglist);
void which_command(int count, char **arglist, int* res);
void SIGINT_handler(int shouldTerminate);
void SIGINT_handler_Parent(void);

/**
 * @brief function to build the shell, with handler to ignore SIGINT to prevent shell from closing. 
 * 
 * @return int 
 */
int prepare(void){
    struct sigaction shell = {
        .sa_handler = SIG_IGN
    };
    if(sigaction(SIGINT, &shell ,NULL)== -1){
        perror("failed init shell");
        exit(1);
    }
    return 0;
}

/**
 * @brief function to execute a background command (with special character "&").
 * 
 * @param count 
 * @param arglist 
 * @return int 
 */
int Background(int count, char **arglist){
    char* cmd;
    int pid;

    cmd = arglist[0];
    // make parent collect background child when done
    SIGINT_handler_Parent();
    pid = fork();
    if(pid < 0){
        perror("failed fork");
        return 0;
    } 
    // Child
    if(pid == 0){
        // make sure ignore SIGINT will happen
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

/**
 * @brief function to execute a recirection command (with special character ">").
 * 
 * @param count 
 * @param arglist 
 * @return int 
 */
int Redirection(int count, char **arglist){
    char *cmd, *to;
    int fileDesc, pid;

    cmd = arglist[0];
    //split commant to 2 parts of - command and file
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
    if(close(fileDesc)== -1){
        perror("failed to close fileDesc");
        return 0;
    }
    // make parent wait until child process is done - no zombies!
    waitpid(pid, NULL, WUNTRACED);   
    return 1;
}

/**
 * @brief function to execute a pipe command (with special character "|").
 * 
 * @param arglist 
 * @param whereIsSym 
 * @return int 
 */
int Pipe(char **arglist, int whereIsSym){
    int p, fd[2], r, w, pid[2];
   char *cmd; 
    
    // split command to 2 parts of pipe 
    arglist[whereIsSym] = NULL;
    cmd = arglist[0];
    p = pipe(fd);
    if(p == -1){
        perror("failed pipe");
        return 0;
    }
    r = fd[0];
    w = fd[1];
    // first child process 
    pid[0] = fork();
    if(pid[0] < 0){
        perror("failed fork");
        return 0;
    }
    // Child 1 - executes first part of command and writes to stdout
    if(pid[0] == 0){
        // make sure process will stop with SIGINT
        SIGINT_handler(1);
         if(close(r) == -1){
            perror("failed to close write");
            exit(1);
        }
        if(dup2(w,1) == -1){
            perror("failed dup2");
            exit(1);
        }
        if(close(w) == -1){
            perror("failed to close read");
            exit(1);
        }
        execvp(cmd, arglist);
        // will only reach this line if execvp fails
        perror("failed execvp");
        exit(1);
    }
    else{
        // back to parent
        cmd = arglist[whereIsSym+1];
        pid[1] = fork();
        // Second child process 
        if(pid[1] < 0){
            perror("failed fork");
            return 0;
        }
        // Child 2 - accecpts input from stdin and executes second part of command 
        if(pid[1] == 0){
             // make sure process will stop with SIGINT
            SIGINT_handler(1);
            if(close(w) == -1){
                perror("failed to close read");
                exit(1);
            }
            if(dup2(r,0) == -1){
                perror("failed dup2");
                exit(1);
            }
            if(close(r) == -1){
                perror("failed to close write");
                exit(1);
            }
            execvp(cmd, arglist + whereIsSym +1);
            // will only reach this line if execvp fails
            perror("failed execvp");
            exit(1);
        }
        // Back to parent
        else{
            if(close(r) == -1){
                perror("failed to close read");
                return 0;
            }
            if(close(w) == -1){
                perror("failed to close write");
                return 0;
            } 
            // make parent wait until all child processes are done - no zombies!
            if(waitpid(pid[0], NULL, 0)==-1 && errno != ECHILD){
                perror("failed waiting for child 1");
                return 0;
            }
            if(waitpid(pid[1], NULL, 0)==-1 && errno != ECHILD){
                perror("failed waiting for child 2");
                return 0;
            }
        }
    }
    return 1;
}

/**
 * @brief function to execute a regular command (no special character).
 * 
 * @param count 
 * @param arglist 
 * @return int 
 */
int Regular(int count, char **arglist){
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
         // make sure process will stop with SIGINT
        SIGINT_handler(1);
        execvp(cmd, arglist);
        // will only reach this line if execvp fails
        perror("failed execvp");
        exit(1);
    }
    // Parent
    // make parent wait until child process is done - no zombies!
    waitpid(pid, NULL, WUNTRACED);   
    return 1;
}

/**
 * @brief main building function of the shell. Will first categorize the command and then call a compatible function for the excecution.
 * 
 * @param count - length of arglist.
 * @param arglist - the command parsed into words.
 * @return int - 1 upon success or 0 upon a failiure in a child process. 
 */
int process_arglist(int count, char **arglist){
    int *whichCmdAndWhere;
    
    whichCmdAndWhere = (int*)calloc(2, sizeof(int));
    which_command(count, arglist, whichCmdAndWhere);
    /**
     * &
     **/
    if(whichCmdAndWhere[0] == 1){
       return Background(count, arglist);
    }
     /**
     * >
     **/
    if(whichCmdAndWhere[0] == 2){
       return Redirection(count, arglist);
    }
     /**
     * |
     **/
    if(whichCmdAndWhere[0] == 3){
        return Pipe(arglist, whichCmdAndWhere[1]);
    }
     /**
     * regular command
     **/
    if(whichCmdAndWhere[0] == 4){
        return Regular(count, arglist);
    }
    return 1;  
}

int finalize(void){
    return 0;
}

/**
 * @brief 
 * In order to understand what is the command type, we will check for the special characters in the locations described at "ASSUMPTIONS" section:
 * & - the last word of the command line, will appear at arglist[count -2]. The function will return command type = 1.
 * > - appears one before last on the command line, will appear at arglist[count -3]. The function will return command type = 2. 
 * | - appears somewhere in the command, so we will scan it. The function will return command type = 3. 
 * regular command - a command with no special character. The function will return command type = 4. 
 * 
 * @param count - length of arglist
 * @param arglist - command parsed into words
 * @param res - res[0] - type of command, res[1] - where is the special character.
 */
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

/**
 * @brief will change signals handler according to shouldTerminate:
 * shouldTerminate = 0 : the process is of type "Background" and should not terminate upon SIGINT -> handler = SIG_IGN.
 * shouldTerminate = 1 : the process is of type "Pipe" / "Regular" / "Redirection" and should terminate upon SIGINT -> handler = SIG_DFL.
 * 
 * @param shouldTerminate 
 */
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
        sig.sa_flags = SA_RESTART;
        sig.sa_handler = SIG_IGN;
        signal = SIGINT;
        changed = sigaction(signal, &sig, NULL);
        if(changed == -1){
            perror("failed signals");
        }
    }
}

/**
 * @brief For the parent of a background child, we will change the sa_handler to ignore SIGCHLD so 
 * that when the background process is finished it won't be a zombie.
 */
void SIGINT_handler_Parent(void){
    struct sigaction sig;
    int signal, changed;
    sig.sa_flags = SA_RESTART;
    sig.sa_handler = SIG_IGN;
    signal = SIGCHLD;
    changed = sigaction(signal, &sig, NULL);
    if(changed == -1){
        perror("failed signals");
    }
}