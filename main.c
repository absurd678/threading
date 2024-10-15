#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdbool.h>
#include <signal.h>
#include <string.h>
#include <sys/wait.h>

/********************************************/
/*           GLOBAL VARIABLES               */
/********************************************/
 
const int maxArguments = 10;
const int BuffSize = 256;
const int forkError = -1;
const int forkChild = 0;
const int execError = -1;

struct ChildFIFO
{
    pid_t PID;
    struct ChildFIFO *next_process;
};

struct ChildFIFO *head_list;

/********************************************/
/*              FUNCTIONS PROTOTYPES        */
/********************************************/

void readCmdInputLine(char** line);
// ---------------------FIFO----------------------------
void add_elem(pid_t new_PID);
void delete_head_list();
int countLength();
void printAll();


//----------------PROCESS FUNCTIONS----------------

void killHeadProcess(int sig);
char **divideIntoWords(char* line);
int recognizeCommands(char **args, char commands[maxArguments][BuffSize]);
void createChildProcess(char **args);
void readCmdInputLine(char** line);
void killAllChildren();


/*--MAIN--*/
int main(){
    char *line = NULL;
    char **lexemCmdMatrix;
    system("clear");

    signal(SIGINT, killHeadProcess);
    printf("\n");

    while(true){
        char cwd[BuffSize];
        getcwd(cwd, sizeof(cwd));
        printf("%s", cwd);
        readCmdInputLine(&line);
        if(strcmp(line, "killall")==0){
            killAllChildren();
            free(line);
            break;
        }
        else if(strcmp(line, "clear")==0){
            system("clear");
            continue;
        }
        else if(line[0]!='\0'){
            lexemCmdMatrix = divideIntoWords(line);
            createChildProcess(lexemCmdMatrix);
            free(lexemCmdMatrix);
        }
    }
    return 0;
}


/*------------------------------------------*/
/* FUNCTION IMPLEMENTATION                  */
/*------------------------------------------*/

//--------------CHILDREN FIFO --------------------------------
void add_elem(pid_t new_PID)
{
    struct ChildFIFO *new_list = (struct ChildFIFO *)malloc(sizeof(struct ChildFIFO));
    new_list->PID = new_PID;
    new_list->next_process = NULL;

    if (head_list == NULL)
    {
        head_list = new_list;
    }
    else
    {
        struct ChildFIFO *current = head_list;
        while (current->next_process != NULL)
        {
            current = current->next_process;
        }
        current->next_process = new_list;
    }
    return;
}

void delete_head_list(){
    if(head_list==NULL){
        return;
    }
    struct ChildFIFO *current = head_list;
    head_list = current->next_process;
    free(current);
    return;
}

int countLength(){
    struct ChildFIFO *current = head_list;
    int count = 0;
    while(current!=NULL){
        count++;
        current = current->next_process;
    }
    return count;
}

void printAll(){
    struct ChildFIFO *current = head_list;
    int i = 1;
    printf("\nChildren processes for parent %d:\n", getpid());
    while(current!=NULL){
        printf("%d. Child process PID: %d\n", i, current->PID);
        current = current->next_process;
        i++;
    }
}

//----------------PROCESS FUNCTIONS----------------

/*--Capture signal Ctrl+C---*/
void killHeadProcess(int sig){ // for Ctrl+C commands
    char cwd[BuffSize];
    
    if (head_list==NULL){ // if no children
        printf("\nParent process killed PID: %d\n", getpid());
        kill(getpid(), SIGTERM);
        waitpid(-1, NULL, 0);
    }
    else // delete the head child
    {
        printAll(); // print all children of current parent
        printf("\nThe process %d was killed", head_list->PID); 
        kill(head_list->PID, SIGKILL);
        waitpid(head_list->PID, NULL, 0);
        delete_head_list();
    }
    
    getcwd(cwd, sizeof(cwd));
    printf("\n%s$", cwd);
    fflush(stdout);
    return;
}

/*--Divide the line into distinct words--*/
char **divideIntoWords(char* line){
    int i = 0;
    int wordStart = 0;
    int lastWord = 0;
    int lengthLine = strlen(line);
    
    char **parts = (char **)malloc((lengthLine+1)*sizeof(char*)); // allocate memory for the parts

    if(parts==NULL){
        return NULL;
    }
    
    while(i<=lengthLine){
        if(line[i]=='\0' || line[i]==' '){ // if end of command or argument
            if(i>wordStart){ // save each word
                int word_length = i - wordStart;
                parts[lastWord] = (char*)malloc((word_length+1)*sizeof(char));

                if(parts[lastWord]==NULL){
                    return NULL;
                }
                strncpy(parts[lastWord], &line[wordStart], word_length);
                parts[lastWord][word_length] = '\0';
                lastWord++;
            }
            wordStart = i+1;
        }
        i++;
    }
    parts[lastWord] = NULL;
    return parts;
}

// collect words into command(s) (by comma separation)
int recognizeCommands(char **args, char commands[maxArguments][BuffSize]){
    int i = 0;
    int counter = 0;

    while(args[i]){
        int j = 0;
        while(args[i][j]!='\0')
            j++;
        if (args[i][j-1]==','){
            args[i][j-1] = '\0';
            strcat(commands[counter], args[i]);
            counter++;
            i++;
            continue;
        }
        strcat(commands[counter], args[i]);
        strcat(commands[counter], " ");
        i++;
    }

    counter++;
    printf("Commands read: %d\n", counter);
    return counter;
}

/*--Create the new process--*/
void createChildProcess(char **args){
    pid_t pid;
    int success = execError;
    int cmdRead = 0; // amount of command
    char** command; // array of command
    int childAmount = 0;

    // if cd command written 
    if(strcmp(args[0], "cd")==0){
        if(args[1]!=NULL){
            if (chdir(args[1])!=0){
                printf("Can't change the directory");
            }
        }
        else {
            printf("No target directory specified\n");
        }
        getchar();
        return;
    }

    // if another command (or set of commands) written
    char commands[maxArguments][BuffSize];
    for (int i = 0; i < maxArguments; i++){
        strcpy(commands[i], "");
    }  
    cmdRead = recognizeCommands(args, commands);
    

    for (int i=0; i<cmdRead; i++){
        command = divideIntoWords(commands[i]);
        pid = fork();
        switch(pid){
            case forkError:
                printf("Can't fork a process\n");
                exit(0);
                break;
            case forkChild:
                printf("Children process created = %d\n", getpid());
                success = execvp(command[0], command);
                if (success==execError){
                    printf("Command %s not found\n", command[0]);
                    kill(getpid(), SIGTERM);
                }
                break;
            default:
                add_elem(pid);
                childAmount = countLength();
                // wait for every child to end
                for (int i=0; i<childAmount; i++){
                    wait(NULL); 
                }
                break;
        }
    }
    getchar();
    return;
}

/*--Read the input line--*/
void readCmdInputLine(char** line){
    size_t len = 0;
    ssize_t read;
    printf("$");
    read = getline(line, &len, stdin);
    if (read==-1){
        printf("Error reading.\n");

    }
    else {
        size_t newLinePos = strcspn(*line, "\n");
        if((*line)[newLinePos]=='\n'){
            (*line)[newLinePos] = '\0';
        }
        if (read-1==0){
            return;
        }
        printf("%zu symbols were read: %s", read-1, *line);
    }
    return;
}

/*--Delete all children processes--*/
void killAllChildren(){
    struct ChildFIFO* current = head_list;
    struct ChildFIFO* next;
    while(current!=NULL){
        next = current->next_process;
        kill(current->PID, SIGTERM);
        waitpid(current->PID, NULL, 0);
        printf("Process %d killed.\n", current->PID);
        free(current);
        current = next;
    }
    head_list = NULL;
}
