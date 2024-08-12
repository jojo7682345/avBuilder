#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/wait.h>

int main(){

    int fd[2];

    if(pipe(fd) == -1){
        printf("\nError: Could not create a pipe!\n");
        exit(-1);
    }

    int cpid = fork();

    if(cpid == -1){
        printf("\nError: Could not fork!\n");
        exit(-1);
    }

    if(cpid == 0){
        char toSend[] = "Hello educative user! <3";
        write(fd[1], toSend, strlen(toSend));
        printf("\nChild: Data sent to parent!\n");
        exit(0);
    }
    else{
        wait(NULL);
        char toRecieve[BUFSIZ];
        read(fd[0], toRecieve, BUFSIZ);
        printf("\nParent: Data from child = %s\n\n", toRecieve);
        exit(0);
    }

}