#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdbool.h>
#include <signal.h>
#include <string.h>
#include <sys/wait.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/resource.h>


/********************************************/
/*           GLOBAL VARIABLES               */
/********************************************/
 
const int maxArguments = 10;
const int BuffSize = 256;
const int forkError = -1;
const int forkChild = 0;
const int execError = -1;
const int BUFFSIZE1024 = 1024;

//------------NICE LEVELS------------
const int lsPriority = 10;
const int catPriority = 0;
const int killallPriority = -5;
const int clearPriority = -5;


// ----------ERROR CONSTANTS ----------
const int ERR_OPN_DIR = 1;
const int ERR_OPN_FILE = 2;
const int ERR_WRT_ARCH = 3;
const int ERR_RD_ARCH = 4;
const int ERR_EXTR_ARCH = 5;
const int ERR_CRT_DIR = 6;
const int ERR_EXTR = 7;
const int ERR_PTH_ARCH = 8;

//-------------------STRUCTURES-----------------
// TODO: make priority field, minPrior var (change each time the queue is changed)
struct ChildFIFO
{
    pid_t PID;
    struct ChildFIFO *nextProcess;
    int niceLevel;
};
struct procInfo
{
    int errCode;
    int priority; 
};

struct ChildFIFO *head_list;

//-------------------VARIABLES----------------
int processError = 0;



/********************************************/
/*              FUNCTIONS PROTOTYPES        */
/********************************************/

void readCmdInputLine(char** line);
char **sliceString(char **full, int start, int end);
char **divideIntoWords(char* line);
int recognizeCommands(char **args, char commands[maxArguments][BuffSize]);
// ---------------------FIFO----------------------------
void add_elem(pid_t new_PID, int priority);
void delete_head_list();
int countLength();
void printAll();
void PrintErr(int errorCode);  


//----------------PROCESS FUNCTIONS----------------
struct procInfo* findCommand(char **command, char cwd[BuffSize], int priority);
void createChildProcess(char **args, char cwd[BuffSize]);

void killHeadProcess(int sig);

void killAllCommand(); // killall
int lsCommand(char *dir); // ls
int catCommand(char *filePath); // cat
int niceCommand(char **command, char cwd[BuffSize]); // nice



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
        lexemCmdMatrix = divideIntoWords(line);
        createChildProcess(lexemCmdMatrix, cwd);
    }
    return 0;
}


/*------------------------------------------*/
/* FUNCTION IMPLEMENTATION                  */
/*------------------------------------------*/

//--------------CHILDREN FIFO --------------------------------
void add_elem(pid_t new_PID, int priority)
{
    struct ChildFIFO *new_list = (struct ChildFIFO *)malloc(sizeof(struct ChildFIFO));
    new_list->PID = new_PID;
    new_list->nextProcess = NULL;
    new_list->niceLevel = priority;

    if (head_list == NULL)
    {
        head_list = new_list;
    }
    else
    {
        struct ChildFIFO *current = head_list;
        while (current->nextProcess != NULL)
        {
            current = current->nextProcess;
        }
        current->nextProcess = new_list;
    }
    return;
}

void delete_head_list(){
    if(head_list==NULL){
        return;
    }
    struct ChildFIFO *current = head_list;
    head_list = current->nextProcess;
    free(current);
    return;
}

int countLength(){
    struct ChildFIFO *current = head_list;
    int count = 0;
    while(current!=NULL){
        count++;
        current = current->nextProcess;
    }
    return count;
}

void printAll(){
    struct ChildFIFO *current = head_list;
    int i = 1;
    printf("\nChildren processes for parent %d:\n", getpid());
    while(current!=NULL){
        printf("%d. Child process PID: %d Nice: %d\n", i, current->PID, current->niceLevel);
        current = current->nextProcess;
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
void createChildProcess(char **args, char cwd[BuffSize]){
    pid_t pid;
    struct procInfo *ans;
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
                ans = findCommand(command, cwd, NULL);
                if (ans->errCode!=0){
                    printf("Command %s not found\n", command[0]);
                    kill(getpid(), SIGTERM); 
                }
                break;
            default:
                add_elem(pid, ans->priority);
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

struct procInfo* findCommand(char **command, char cwd[BuffSize], int priority){
    struct procInfo ans;
    if (sizeof(command)==1){ // if ls, killall and other 1-word commands
    if(strcmp(command[0], "killall")==0){
        setpriority(PRIO_PROCESS, 0, killallPriority);
        killAllCommand();
        ans.errCode = 0; ans.priority = killallPriority;
        return &ans;
    }
    else if(strcmp(command[0], "clear")==0){
        setpriority(PRIO_PROCESS, 0, clearPriority);
        system("clear");
        ans.errCode = 0; ans.priority = clearPriority;
        return &ans;
    }
    else if(strcmp(command[0], "ls")==0){
        if (priority==NULL)
            priority = lsPriority;
        setpriority(PRIO_PROCESS, 0, priority);
        if((processError = lsCommand(cwd))!=0){
            PrintErr(processError);
        }
        ans.errCode = 0; ans.priority = priority;
        return &ans;
    } }
    else if(command[0]!='\0'){
        if(strcmp(command[0], "cat")==0){
            if (priority==NULL)
                priority = catPriority;
            setpriority(PRIO_PROCESS, 0, priority);
            char *filePath;
            strcpy(filePath, cwd);
            strcat(filePath, "/");
            strcat(filePath, command[1]);
            if ((processError = catCommand(filePath)) != 0)
            {
                PrintErr(processError);
            }
            ans.errCode = 0; ans.priority = priority;
            return &ans;
        }
        else if (strcmp(command[0], "nice")==0){
            niceCommand(command, cwd);
            ans.errCode = 0; ans.priority = priority;
            return &ans;
        }
    }
    ans.errCode = 0; ans.priority = priority;
    return &ans;
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
void killAllCommand(){
    struct ChildFIFO* current = head_list;
    struct ChildFIFO* next;
    while(current!=NULL){
        next = current->nextProcess;
        kill(current->PID, SIGTERM);
        waitpid(current->PID, NULL, 0);
        printf("Process %d killed.\n", current->PID);
        free(current);
        current = next;
    }
    head_list = NULL;
}


int lsCommand(char *dir){
    DIR *dp;
    struct dirent *entry;
    struct stat statbuf;
    int errCode = 0;

    if ((dp = opendir(dir)) == NULL)
    {
        return ERR_OPN_DIR;
    }
    chdir(dir);
    while ((entry = readdir(dp)) != NULL)
    {
        lstat(entry->d_name, &statbuf);

        // Ignore .. and .
        if (strcmp(".", entry->d_name) == 0 || strcmp("..", entry->d_name) == 0)
            continue;

        printf("\n%s", entry->d_name);
    }
    closedir(dp);
    return 0;
}

int catCommand(char *filePath)
{
    int errCode = 0;


    FILE *fin = fopen(filePath, "r");

    if (!fin)
    {
        return ERR_OPN_FILE;
    }

    fseek(fin, 0, SEEK_END);
    fseek(fin, 0, SEEK_SET);
    char buffer[BUFFSIZE1024];
    size_t total_bytes_read = 0;
    printf("\n");
    while ((total_bytes_read = fread(buffer, 1, BUFFSIZE1024, fin)) > 0)
    {
        printf("%s\n", buffer);
    }

    fclose(fin);
    return 0;
}

int niceCommand(char **command, char cwd[BuffSize]){
    pid_t pid;
    int newPriority;

    pid = fork();
    if(pid==forkError){
        printf("Can't fork a process\n");
        exit(0);
    }
    else if(pid==forkChild){
        printf("Child process created = %d\n", getpid());
        if (sizeof(command)==4){
            if (strcmp(command[1], "-n")!=0){
                return 0;//NICE_BAD_OPTION;
            }
            newPriority = atoi(command[2]);
            if (newPriority >= -20 && newPriority <= 19){
                printf("Priority of %s set to %d\n", command[4], newPriority);
                char **solidCommand = sliceString(command, 4, sizeof(command)-1);
                findCommand(solidCommand, cwd, newPriority);
            } else {
                return 0;//NICE_BAD_PRIORITY;
            }

        } else {
            return 0;//NICE_BAD_ARGUMENTS;
        }
        getchar();
        exit(0);
    }
    else{
        waitpid(pid, NULL, 0);
    }
    return 1;
}

char **sliceString(char **full, int start, int end){
    char **sliced = (char **)calloc(end - start + 2, sizeof(char *));
    int i;
    for (i = start; i <= end; i++)
    {
        sliced[i - start] = full[i];
    }
    sliced[i - start] = NULL;
    return sliced;
}


void PrintErr(int errorCode)
{
    switch (errorCode)
    {
    case ERR_OPN_DIR:
        fprintf(stderr, "Error opening directory\n");
        break;
    case ERR_OPN_FILE:
        fprintf(stderr, "Error opening file\n");
        break;
    case ERR_WRT_ARCH:
        fprintf(stderr, "Error writing to archive file\n");
        break;
    case ERR_RD_ARCH:
        fprintf(stderr, "Error reading archived data\n");
        break;
    case ERR_EXTR_ARCH:
        fprintf(stderr, "Error extracting archived data\n");
        break;
    case ERR_CRT_DIR:
        fprintf(stderr, "Error creating directory\n");
        break;
    case ERR_EXTR:
        fprintf(stderr, "Error: not a directory requested to extract\n");
        break;
    case ERR_PTH_ARCH:
        fprintf(stderr, "Error of extraction: incorrect archive path\n");
        break;
    default:
        fprintf(stderr, "Unknown error code: %d\n", errorCode);
        break;
    }
}