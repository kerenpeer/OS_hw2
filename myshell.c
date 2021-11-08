int prepare(void);
int process_arglist(int count, char **arglist);
int finalize(void);

int prepare(void){

}

int process_arglist(int count, char **arglist){
    int *whichCmdAndWhere,fileDesc,pid;
    char *cmd, *to;
    
    whichCmdAndWhere = (int*)calloc(2, sizeof(int));
    which_command(count, arglist, whichCmdAndWhere);
    cmd = (char*)malloc(whichCmdAndWhere[1] * sizeof(char));
    /**
     * &
     **/
    if(whichCmdAndWhere[0] == 1){
        //add
    }
     /**
     * >
     **/
    if(whichCmdAndWhere[0] == 2){
        cmd = arglist[0];
        arglist[count-2] = NULL;
        to = arglist[count-1];
        fileDesc = open(to,O_RDWR | O_CREAT, S_IWUSR);
        if(fileDesc < 0){
            perror("failed to open file");
            exit(1);
        }
        pid = fork();
        if(pid < 0){
            perror("failed fork");
            exit(1);
        }
        if(pid == 0){
          //add SIGINT handeling
          if(dup2(fileDesc,1) == -1){
              perror("failed dup2");
              exit(1);
          }
          close(fileDesc);
          execvp(cmd, arglist);
          // will only reach this line if execvp fails
          perror("failed execvp");
          exit(1);
        }
        close(fileDesc);
        // make parent wait until child process is done - no zombies!
        wait(NULL);
    }
     /**
     * |
     **/
    if(whichCmdAndWhere[0] == 3){   
        //add
    }
     /**
     * regular command
     **/
    if(whichCmdAndWhere[0] == 4){
        cmd = arglist[0];
        pid = fork();
        if(pid < 0){
            perror("failed fork");
            exit(1);
        }
        if(pid == 0){
            //add SIGINT handeling
            execvp(cmd, arglist);
            // will only reach this line if execvp fails
            perror("failed execvp");
            exit(1);
        }
        // make parent wait until child process is done - no zombies!
        wait(NULL);
    }
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
    if (strcmp(arglist[count -1],"&") == 0){
       res[0] = 1;
       res[1] = count -1;
    }
    if (strcmp(arglist[count -2], ">") == 0){
        res[0] = 2;
        res[1] = count -2;
    }
    for( i = 0; i < count; i++){
        if(strcmp(arglist[i], "|") == 0){
            res[0] = 3;
            res[1] = i;
        }
    }
    res[0] = 4;
    res[1] = 0;
    return ;
}

void SIGINT_handler(int isChild){
    struct sigaction sig{
        .sa_flags = SA_RESTART;
    };
    if(isChild == 1){
        sig.sa_flags = SIG_IGN;
    }
     if(isChild == 0){
        sig.sa_flags = SIG_DFL;
    }
    //deal with error
}