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
const int OthrApp = 0;
const int NoPriority = -100;


// ----------ERROR CONSTANTS ----------
const int ERR_OPN_DIR = 1;
const int ERR_OPN_FILE = 2;
const int ERR_WRT_ARCH = 3;
const int ERR_RD_ARCH = 4;
const int ERR_EXTR_ARCH = 5;
const int ERR_CRT_DIR = 6;
const int ERR_EXTR = 7;
const int ERR_PTH_ARCH = 8;
const int NICE_BAD_OPTION = 9;
const int NICE_BAD_PRIORITY = 10;
const int NICE_BAD_ARGUMENTS = 11;
const int ERR_PROC_UNFOUND = 12;
const int ERR_MEM = 13;

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
int findCommand(char **command, char cwd[BuffSize], int* priority);
void createChildProcess(char **args, char cwd[BuffSize]);

void killHeadProcess(int sig);

void killAllCommand(); // killall
int lsCommand(char *dir); // ls
int catCommand(char *filePath); // cat
int niceCommand(char **command, char cwd[BuffSize], int *newPriority); // nice



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
            killAllCommand();
            free(line);
            break;
        } 
        else if(strcmp(line, "clear")==0){
            system("clear");
            continue;
        }
        else if(line[0]!='\0'){
            lexemCmdMatrix = divideIntoWords(line);
            createChildProcess(lexemCmdMatrix, cwd);
            free(lexemCmdMatrix);
        }
        
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
    printf("\nAll processes:\n");
    while(current!=NULL){
        printf("%d. Process PID: %d Nice: %d\n", i, current->PID, current->niceLevel);
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
// ASK CHAT GPT WHY DOES NOT IT WORK. WHAT IS THE DIFFERENCE BETWEEN THIS FUNCTION AND THE OLD ONE. 
// WHY THE FIFO REMAINS EMPTY?!
void createChildProcess(char **args, char cwd[BuffSize]){
    pid_t pid;
    int findProcError = 0;
    int cmdRead = 0; // amount of command
    char** command; // array of command
    int childAmount = 0;
    
    int childPriority = NoPriority;
    int* findPriority = &childPriority;
    
    

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
                printf("Children process created = %d with name: %s\n", getpid(), command[0]);
                findProcError = findCommand(command, cwd, findPriority);
                if (findProcError!=0){
                    *findPriority = OthrApp;
                    setpriority(PRIO_PROCESS, 0, OthrApp);
                    findProcError = execvp(command[0], command);

                    if (findProcError ==-1) {
                        printf("Command %s not found\nPrintErr:\n", command[0]);
                        PrintErr(findProcError);
                        kill(getpid(), SIGTERM);
                        exit(NoPriority);
                        break;
                    }
                     
                }
                // Передаем приоритет в родительский процесс
                exit(*findPriority);
                break;
            default:
                int status;
                waitpid(pid, &status, 0);
                int findPriority = WEXITSTATUS(status);
                if (findPriority!=NoPriority){
                    add_elem(pid, findPriority);
                }
                printAll();
                break;
        }
    }
    //getchar();
    
    return;
}


int findCommand(char **command, char cwd[BuffSize], int* priority){
    int ans;
    
    if(strcmp(command[0], "killall")==0){ // kill all processes
        setpriority(PRIO_PROCESS, 0, killallPriority);
        killAllCommand();
        *priority = killallPriority;
        return 0;
    }
    else if(strcmp(command[0], "clear")==0){ // clear 
        setpriority(PRIO_PROCESS, 0, clearPriority);
        system("clear");
        *priority = clearPriority;
        return ans;
    }
    else if(strcmp(command[0], "ls")==0){ // ls
        if (*priority==NoPriority)
            *priority = lsPriority;
        setpriority(PRIO_PROCESS, 0, *priority);
        if((processError = lsCommand(cwd))!=0){
            return processError;
        }
        return 0;
    } 
    else if(strcmp(command[0], "cat")==0){ // cat
        if (*priority==NoPriority)
            *priority = catPriority;
        setpriority(PRIO_PROCESS, 0, *priority);
        char *filePath = (char*)malloc(strlen(cwd)+strlen(command[1])+1);
        if (filePath == NULL) {
            perror("malloc failed");
            return ERR_MEM;
        }

        strcpy(filePath, cwd);
        strcat(filePath, "/");
        strcat(filePath, command[1]);
        if ((processError = catCommand(filePath)) != 0)
        {
            free(filePath);
            return processError;
        }
        free(filePath);
        return 0;
    }
        
    else if (strcmp(command[0], "nice")==0){ // nice command
        ans = niceCommand(command, cwd, priority);
        return ans;
    }    
/*
    else { // Launch other applications (firefox, etc)
        if (*priority==NoPriority)
            *priority = OthrApp;
        setpriority(PRIO_PROCESS, 0, *priority);
        ans = execvp(command[0], command);
    }
*/  
    return ERR_PROC_UNFOUND;
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


int lsCommand(char dir[BuffSize]){
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
        fwrite(buffer, 1, total_bytes_read, stdout);
    }

    fclose(fin);
    return 0;
}

int niceCommand(char **command, char cwd[BuffSize], int *newPriority){
    
    if (sizeof(command)>=4){
        if (strcmp(command[1], "-n")!=0){
            return NICE_BAD_OPTION;
        }
        *newPriority = atoi(command[2]);
        if (*newPriority >= -20 && *newPriority <= 19){
            printf("\nPriority of %s set to %d\n", command[3], *newPriority);
            char **solidCommand = sliceString(command, 3, sizeof(command)-1);
            
            return findCommand(solidCommand, cwd, newPriority);
        } else {
            return NICE_BAD_PRIORITY;
        }

    } else {
        return NICE_BAD_ARGUMENTS;
    }
    
    
    return 0;
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