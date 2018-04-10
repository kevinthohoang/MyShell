#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define ARRAY_SIZE 1024

// PATH VARIABLES
char path_line[ARRAY_SIZE];
char path[ARRAY_SIZE];

// USER INPUT VARIABLES
char * pipes[ARRAY_SIZE];
char   user_input[ARRAY_SIZE];
int    total_pipes  = 0;
int    current_pipe = 0;

// EXECUTION VARIABLES
char * separated_pipe[ARRAY_SIZE];
char * arguments[ARRAY_SIZE];
char   executable[ARRAY_SIZE];
char   i_redirect[ARRAY_SIZE];
char   o_redirect[ARRAY_SIZE];

// PIPING VARIABLES
int pipefd[2];
int pipefd_in = 0;

void error(char * message)
{
    fprintf(stderr, "%s\n", message);
    exit(-1);
}

void get_path_line(char * envp[])
{
    for (int i = 0; envp[i] != '\0'; ++i)
	if (strspn(envp[i], "PATH=") == 5)
	    strcpy(path_line, envp[i] + 5);
}

void trim_whitespace(char * pipe)
{
    if (pipe[0] == ' ')
        strcpy(pipe, pipe + 1);

    if (pipe[strlen(pipe) - 1] == ' ')
        pipe[strlen(pipe) - 1] = '\0';
}

void check_whitespace()
{
    for (int i = 0; pipes[i] != '\0'; ++i)
        trim_whitespace(pipes[i]);
}

void split_input()
{
    char * token = strtok(user_input, "|");

    while (token != NULL)
    {
        pipes[total_pipes++] = token;
        token = strtok(NULL, "|");
    }
}

void get_user_input()
{
    printf("%% ");
    fgets(user_input, 1024, stdin);
    user_input[strlen(user_input) - 1] = '\0';

    split_input();
    check_whitespace();
}

int check_separated_pipes(char * search)
{
    for (int i = 0; separated_pipe[i] != '\0'; ++i)
        if (strcmp(separated_pipe[i], search) == 0)
            return i;
    
    return -1;
}

void get_arguments()
{
    int i;

    for (i = 0; separated_pipe[i] != '\0' && strcmp(separated_pipe[i], ">") != 0 && strcmp(separated_pipe[i], "<") != 0; ++i)
        arguments[i] = separated_pipe[i];

    arguments[i] = '\0';
}

void get_executable()
{
    strcpy(executable, separated_pipe[0]);
}

void get_input_redirect()
{
    int index = check_separated_pipes("<");

    if (index != -1)
        strcpy(i_redirect, separated_pipe[index + 1]);
}

void get_output_redirect()
{
    int index = check_separated_pipes(">");

    if (index != -1)
        strcpy(o_redirect, separated_pipe[index + 1]);
}

void setup()
{
    get_arguments();
    get_executable();
    get_input_redirect();
    get_output_redirect();
}

/* EXCEEDING 5 LINES BY 2 */
void get_next_pipe()
{
    int index = 0;
    char * token = strtok(pipes[current_pipe], " ");

    while (token != NULL)
    {
        separated_pipe[index++] = token;
        token = strtok(NULL, " ");
    }
    separated_pipe[index++] = '\0';

    setup();
}

void redirect_input()
{
    int ifp = open(i_redirect, O_RDONLY);
    if (ifp == -1)
	error("Unable to open input file");
    dup2(ifp, 0);

    close(ifp);
}

void redirect_output()
{
    int ofp = open(o_redirect, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR);
    if (ofp == -1)
	error("Unable to open output file");
    dup2(ofp, 1);

    close(ofp);
}

int check_executable()
{
    struct stat myStat;

    if (stat(executable, &myStat) != -1 && !myStat.st_mode & !S_IXUSR)
	return 1;

    return 0;
}

/* EXCEEDING 5 LINES BY 2 */
int check_path()
{
    struct stat myStat;
    char * 	token = strtok(path_line, ":");

    while (token != NULL)
    {
	sprintf(path, "%s/%s\0", token, executable);
	if (stat(path, &myStat) != -1 && myStat.st_mode & S_IXUSR)
	    return 1;
	token = strtok(NULL, ":");
    }

    return 0;
}

void execute()
{
    if (check_executable() == 1)
	execv(executable, arguments);
    if (check_path() == 1)
	execv(path, arguments);
}

void no_pipes()
{
    get_next_pipe();
    if (strlen(i_redirect) != 0)
	redirect_input();
    if (strlen(o_redirect) != 0)
	redirect_output();

    execute();
}

void first_pipe()
{
    if (strlen(i_redirect) != 0)
	redirect_input();

    dup2(pipefd[1], 1);
    close(pipefd[0]);
    execute();
}

void middle_pipes()
{
    dup2(pipefd_in, 0);
    dup2(pipefd[1], 1);
    close(pipefd[0]);
    execute();
}

void last_pipe()
{
    if (strlen(o_redirect) != 0)
	redirect_output();

    dup2(pipefd_in, 0);
    close(pipefd[0]);
    execute();
}

/* EXCEEDING 5 LINES BY 2 */
void handle_pipes()
{
    int pid;

    while (current_pipe != total_pipes)
    {
	pipe(pipefd);
	get_next_pipe();

	switch (pid = fork())
	{
	    case -1:
		error("Fork failed");
	    case 0:
		if (current_pipe == 0)
		    first_pipe();
		else if (current_pipe != total_pipes - 1)
		    middle_pipes();
		else
		    last_pipe();
	    default:
		wait(NULL);
		close(pipefd[1]);
		pipefd_in = pipefd[0];
		++current_pipe;
	}
    }
}

void handle_execution()
{
    if (total_pipes == 1)
	no_pipes();
    else
	handle_pipes();
}

void run()
{
    int pid;

    switch (pid = fork())
    {
	case -1:
	    error("Fork failed");
	case 0:
	    handle_execution();
	    break;
	default:
	    wait(NULL);
    }
}

int main(int argc, char * argv[], char * envp[])
{
    get_user_input();
    get_path_line(envp);
    run();

    return 0;
}
