#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdbool.h>
#include <signal.h>
#include <string.h>
#include <sys/wait.h>

/********************************************/
/*           ГЛОБАЛЬНЫЕ ПЕРЕМЕННЫЕ          */
/********************************************/

struct PID_list
{
    pid_t PID;
    struct PID_list *next_process;
};

struct PID_list *head_list;

/********************************************/
/*              РЕАЛИЗАЦИЯ ФУНКЦИЙ          */
/********************************************/

/*------------------------------------------*/
/* Добавление нового элемента (дочернего процесса) в список */
/*------------------------------------------*/
void add_elem(pid_t new_PID)
{
    struct PID_list *new_list = (struct PID_list *)malloc(sizeof(struct PID_list));
    new_list->PID = new_PID;
    new_list->next_process = NULL;

    if (head_list == NULL)
    {
        head_list = new_list;
    }
    else
    {
        struct PID_list *current = head_list;
        while (current->next_process != NULL)
        {
            current = current->next_process;
        }
        current->next_process = new_list;
    }
    return;
}

/*--FIFO--*/
void delete_head_list(){
    if(head_list==NULL){
        return;
    }
    struct PID_list *current = head_list;
    head_list = current->next_process;
    free(current);
    return;
}

/*--Capture signal---*/
void Ctrl_plus_C(int sig){
    // no children
    if (head_list==NULL){
        printf("\nParent process killed PID: %d\n", getpid());
        kill(getpid(), SIGTERM);
        waitpid(-1, NULL, 0);
    }
    else 
    {
        struct PID_list *current = head_list;
        int i = 1;
        printf("\nChildren processes for parent %d:\n", getpid());
        while(current!=NULL){
            printf("%d. Child process PID: %d\n", i, current->PID);
            current = current->next_process;
            i++;
        }  
        printf("\nThe process %d was killed", head_list->PID); // why head_list??
        kill(head_list->PID, SIGKILL);
        waitpid(head_list->PID, NULL, 0);
        delete_head_list();
    }
    char cwd[256]; // TODO: NO NUMBERS
    getcwd(cwd, sizeof(cwd));
    printf("\n%s\033[0m \033[38;5;63m$\033[0m", cwd);
    fflush(stdout);
    return;
}

/*--Divide the line into distinct lexems--*/
char **word_division(char* line){
    int length = strlen(line);
    char parts = (char)malloc((length+1)*sizeof(char*));

    if(parts==NULL){
        return NULL;
    }
    int i = 0;
    int word_start = 0;
    int word_count = 0;
    while(i<=length){
        if(line[i]==''||line=='\0'){
            if(i>word_start){
                int word_length = i - word_start;
                parts[word_count] = (char*)malloc((word_length+1)*sizeof(char));

                if(parts[word_count]==NULL){
                    return NULL;
                }
                strncpy(parts[word_count], &line[word_start], word_length);
                parts[word_count][word_length] = '\0';
                word_count++;
            }
            word_start = i+1;
        }
        i++;
    }
    parts[word_count] = NULL;
    return parts;
}

/*--Create the new process--*/
void create_new_process(char **con_args){
    pid_t pid;
    int success = -1;
    if(strcmp(con_args[0], "cd")==0){
        if(con_args[1]!=NULL){
            if (chdir(con_args[1]!=0)){
                printf("Can't change the directory");
            }
        }
        else {
            printf("No target directory specified\n");
        }
        getchar();
        return;
    }

    int counter = 0;
    char commands[10][256]; // TODO: NO NUMBERS
    for (int i = 0; i < 10; i++){
        strcpy(commands[i], "");
    }
    int i = 0;
    while(con_args[i]){
        int j = 0;
        while(con_args[i][j]!='\0')
            j++;
        if (con_args[i][j-1]==','){
            con_args[i][j-1] = '\0';
            strcat(commands[counter], con_args[i]);
            counter++;
            i++;
            continue;
        }
        strcat(commands[counter], con_args[i]);
        strcat(commands[counter], " ");
        i++;
    }

    counter++;
    printf("Commands read: %d\n", counter);
    char** con_args_new;
    for (int i=0; i<counter; i++){
        con_args_new = word_division(commands[i]);
        pid = fork();
        switch(pid){
            case -1:
            printf("Can't fork a process\n");
            exit(0);
            break;
            case 0:
            printf("Children process created = %d\n", getpid());
            success = execvp(con_args_new[0], con_args_new);
            if (success==-1){
                printf("Command %s not found\n", con_args_new[0]);
                kill(getpid(), SIGTERM);
            }
            break;
            default:
            add_elem(pid);
            sleep(1);
            continue;
            break;
        }
    }
    getchar();
    return;
}

/*--Read the input line--*/
void read_line(char** line){
    size_t len = 0;
    ssize_t read;
    printf("033[38;5;63m$\033[0m");
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
void kill_children(){
    struct PID_list* current = head_list;
    struct PID_list* next;
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

/*--main program--*/
int main(){
    char *line = NULL;
    char **con_args;
    system("clear");

    signal(SIGINT, Ctrl_plus_C);
    printf("\n");

    while(true){
        char cwd[256];
        getcwd(cwd, sizeof(cwd));
        printf("%s", cwd);
        read_line(&line);
        if(strcmp(line, "killall")==0){
            kill_children();
            free(line);
            break;
        }
        else if(strcmp(line, "clear")==0){
            system("clear");
            continue;
        }
        else if(line[0]!='\0'){
            con_args = word_division(line);
            create_new_process(con_args);
            free(con_args);
        }
    }
    return 0;
}