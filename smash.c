#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <fcntl.h>

#define MAX_INPUT_LENGTH 1024
#define MAX_ARGS 64
char error_message[30] = "An error has occurred\n";


/// description: Takes a line and splits it into args similar to how argc
///              and argv work in main
/// line:        The line being split up. Will be mangled after completion
///              of the function<
/// args:        a pointer to an array of strings that will be filled and
///              allocated with the args from the line
/// num_args:    a point to an integer for the number of arguments in args
/// return:      returns 0 on success, and -1 on failure
int lexer(char *line, char ***args, int *num_args){
    *num_args = 0;
    // count number of args
    char *l = strdup(line);
    if(l == NULL){
        return -1;
    }
    char *token = strtok(l, " \t\n");
    while(token != NULL){
        (*num_args)++;
        token = strtok(NULL, " \t\n");
    }
    free(l);
    // split line into args
    *args = malloc(sizeof(char **) * (*num_args+1));
    //*args[*num_args] = NULL;
    *num_args = 0;
    token = strtok(line, " \t\n");
    while(token != NULL){
        char *token_copy = strdup(token);
        if(token_copy == NULL){
            return -1;
        }
        (*args)[(*num_args)++] = token_copy;
        token = strtok(NULL, " \t\n");
    }
    return 0;
}

int checkArgs(char *args[], int num_args)
{
    int redirect = -1;
    int out = 0;
    for (int i = 0; i < num_args; i++)
    {
        if (strcmp(args[i], ">")==0)
        {
            if (redirect == 0)
            {
                return -1;
            }
            out =  i;
            redirect = 0;

        }
        
    }

    if (redirect == 0)
    {
        if (num_args<3)
        {
            return -1;
        }
        if (out==0|out==num_args-1)
        {
            return -1;
        }
    
    }

    return 0;
    
    
}

void pipes(char *args[], int num_args, int pipe_index)
{
    int redirect = -1;
    char fileName[100];
    int out;


    for(int i = 0; i<num_args; i++)
    {
        if (strcmp(args[i], ">") == 0)
        {
            if (redirect == 0)
            {
                write(STDERR_FILENO, error_message, strlen(error_message));
                return;
            }
            
            redirect = 0;
            out = i;
        }     
          
    }
    if (redirect == 0)
    {
        if (out == num_args - 1)
        {
            write(STDERR_FILENO, error_message, strlen(error_message));
            return;
        }
           
        strcpy(fileName, args[out+1]);
    }



    args[pipe_index] = NULL;
    int pipefd[2];
    if (pipe(pipefd)==-1)
    {
        write(STDERR_FILENO, error_message, strlen(error_message));
        exit(1);
    }

    int pid1 = fork();
    if (pid1 < 0)
    {
        write(STDERR_FILENO, error_message, strlen(error_message));
        exit(1);
    }
    if (pid1 == 0)
    {
        dup2(pipefd[1], 1);
        close(pipefd[0]);
        execv(args[0], args);
        write(STDERR_FILENO, error_message, strlen(error_message));
        return;

    }
    else
    {
        int status;
        waitpid(pid1, &status, 0);
    }

    int pid2 = fork();
    if(pid2<0)
    {
        write(STDERR_FILENO, error_message, strlen(error_message));
        exit(1);
    }
    if (pid2 == 0)
    {
        dup2(pipefd[0], 0);
        close(pipefd[1]);


        //child process 1
       // printf("%s %s\n", args[pipe_index+1], args[pipe_index+2]);
       if (redirect == 0)
        {
            args[out] = NULL;
            close(STDOUT_FILENO); 
            int fd = open(fileName, O_CREAT|O_WRONLY|O_TRUNC, S_IRWXU);
            dup2(fd, STDOUT_FILENO);
            dup2(fd, STDERR_FILENO);
        }


        execv(args[pipe_index+1], (args + pipe_index + 1));
        write(STDERR_FILENO, error_message, strlen(error_message));
        exit(1);
        return;

    }
    else
    {
        close(pipefd[0]);
        close(pipefd[1]);
        int status;
        waitpid(pid2, &status, 0);
    }
}

//TODO Maybe use this at some point
void pipe_smash(char *args[], int num_args, int num_commands)
{
    
    int pipes[num_commands-1][2];
    pid_t pids[num_commands];

    // Create the pipes
    for (int i = 0; i < num_commands-1; i++) {
        if (pipe(pipes[i]) == -1) {
            perror("pipe");
            return;
        }
    }

    // Create the child processes
    for (int i = 0; i < num_commands; i++) {
        pids[i] = fork();
        if (pids[i] == -1) {
            perror("fork");
            return;
        } else if (pids[i] == 0) {
            // Child process
            if (i == 0) {
                // First command: /bin/cat file.txt
                close(pipes[0][0]); // Close the read end of the first pipe
                dup2(pipes[0][1], STDOUT_FILENO); // Redirect stdout to the write end of the first pipe
                close(pipes[0][1]); // Close the write end of the first pipe
                execl("/bin/cat", "/bin/cat", "file.txt", NULL);
            } else if (i == num_commands-1) {
                // Last command: /usr/bin/wc -l
                close(pipes[i-2][1]); // Close the write end of the previous pipe
                dup2(pipes[i-2][0], STDIN_FILENO); // Redirect stdin to the read end of the previous pipe
                close(pipes[i-2][0]); // Close the read end of the previous pipe
                execl("/usr/bin/wc", "/usr/bin/wc", "-l", NULL);
            } else {
                // Middle commands: /usr/bin/sort
                close(pipes[i-1][1]); // Close the write end of the previous pipe
                dup2(pipes[i-1][0], STDIN_FILENO); // Redirect stdin to the read end of the previous pipe
                close(pipes[i-1][0]); // Close the read end of the previous pipe
                close(pipes[i][0]); // Close the read end of the current pipe
                dup2(pipes[i][1], STDOUT_FILENO); // Redirect stdout to the write end of the current pipe
                close(pipes[i][1]); // Close the write end of the current pipe
                execl("/usr/bin/sort", "/usr/bin/sort", NULL);
            }
        }
    }

    // Parent process
    for (int i = 0; i < num_commands-1; i++) {
        close(pipes[i][0]); // Close the read end of each pipe
        close(pipes[i][1]); // Close the write end of each pipe
    }

    // Wait for the last child process to finish
    int status;
    waitpid(pids[num_commands-1], &status, 0);

    return ;
}


void smash(char *args[], int num_args)
{

    if (checkArgs(args, num_args)==-1)
    {
        write(STDERR_FILENO, error_message, strlen(error_message));
        return;
    }
    

    int redirect = -1;
    int out;
    char fileName[100];
    int pipe = -1;
    int pipe_index;

    //checking for redirection
    for(int i = 0; i<num_args; i++)
    {
        if (strcmp(args[i], ">") == 0)
        {
            if (redirect == 0)
            {
                write(STDERR_FILENO, error_message, strlen(error_message));
                return;
            }
            
            redirect = 0;
            out = i;
        }     
        if (strcmp(args[i], "|") == 0)
        {
            pipe = 0;
            pipe_index = i;
        }
        
    }
    if (redirect == 0)
    {
        if (out == num_args - 1)
        {
            write(STDERR_FILENO, error_message, strlen(error_message));
            return;
        }
           
        strcpy(fileName, args[out+1]);
    }
    if (pipe == 0)
    {
        pipes(args, num_args, pipe_index);
        return;
    }
    

    // Handle if the user just enters nothing
    if (args[0] == NULL)
    {
        return;
    }

    // Handle built-in commands
    if (strcmp(args[0], "exit") == 0) 
    {
        //It is an error to pass any arguments to exit
        if(num_args != 1)
        {
            write(STDERR_FILENO, error_message, strlen(error_message));
            return;
        }
        exit(0);  // Exit the loop and terminate the shell
    } 
    else if (strcmp(args[0], "cd") == 0) 
    {
        if (num_args != 2) {
            write(STDERR_FILENO, error_message, strlen(error_message));
            return;
        }
        if (chdir(args[1]) != 0) {
            write(STDERR_FILENO, error_message, strlen(error_message));
            return;
        }
        return;
        
    } 
    else if (strcmp(args[0], "pwd") == 0) 
    {
        char cwd[1024];
        if (getcwd(cwd, 1024) == NULL) {
            write(STDERR_FILENO, error_message, strlen(error_message));
        } else {
            printf("%s\n", cwd);
        }
        return;
    } 
    else if(strcmp(args[0], "loop") == 0)
    {
        if (num_args < 3) 
        {
            write(STDERR_FILENO, error_message, strlen(error_message));
            return;
        }
        int count = atoi(args[1]);
        if (count <= 0) 
        {
            write(STDERR_FILENO, error_message, strlen(error_message));
            return;
        }
        for(int i = 0; i < count; i++)
        {
            smash(args+2, num_args-2);
        }
        return;
    }


    // Create a new process to run the command
    pid_t pid = fork();
    if (pid < 0) {
        write(STDERR_FILENO, error_message, strlen(error_message));
        exit(1); //maybe continue??
    } else if (pid == 0) {

       //file reading
        int fd;
        if (redirect == 0)
        {
            args[out] = NULL;
            close(STDOUT_FILENO); 
            fd = open(fileName, O_CREAT|O_WRONLY|O_TRUNC, S_IRWXU);
            dup2(fd, STDOUT_FILENO);
            dup2(fd, STDERR_FILENO);
        }
        
        execv(args[0], args);
        write(STDERR_FILENO, error_message, strlen(error_message));
        exit(1);
    } else {
        // Parent process
        int status;
        waitpid(pid, &status, 0);
    }
        
    
}


int main(int argc, char *argv[]) {
    if (argc > 1) {
        //TODO
        printf("%s\n", "Bruh");
        return 1;
    }
    while(1)
    {
        //char input[MAX_INPUT_LENGTH];
        char **args;
        int num_args = 0;
        char *input = NULL;
        size_t bufsize = 100;
        ssize_t linelen;

        fprintf(stdout,"smash> ");
        fflush(stdout);
        getline(&input, &bufsize, stdin);
        int size = strlen(input);
        char *tok = NULL;

        while((tok = strsep(&input, ";")) != NULL)
        {
	        lexer(tok, &args, &num_args);
            args[num_args] =  NULL;
            smash(args, num_args);
        }

    }
    

    return 0;
}
