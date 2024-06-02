#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include "TinyFS_errno.h"

#define BLOCKSIZE 256

typedef struct Node{
    int diskNum;
    FILE* fd;
    int nBytes;
    struct Node* next;
}Node;

// GLOBAL VARIABLES

// global unique disk number
int diskNumber = 0;
// we need a way to store the fd, and nbytes
// use a global linked list implementation
Node* diskList = NULL;
Node* cur = NULL;


// DISKLIST HELPER FUNCTIONS
static Node* createNode(int diskNum, FILE* fd, int nBytes) {
    Node* newNode = (Node*)malloc(sizeof(Node));
    if (newNode == NULL) {
        perror("malloc: "); 
        exit(1);
    }
    newNode->diskNum = diskNum;
    newNode->nBytes = nBytes;
    newNode->fd = fd;
    newNode->next = NULL;
    return newNode;
}

static int insert(int diskNum, FILE* fd, int nBytes) {
    Node* newNode = createNode(diskNum, fd, nBytes);
    if (cur == NULL) {
        diskList = newNode;
        cur = diskList;
    }else{
        cur->next = newNode;
        cur = cur->next;
    }
    return 0;
}

static int deleteNode(int disk){
    Node* temp = diskList;
    Node* prev = NULL;

    if (temp != NULL && temp->diskNum == disk){
        diskList = temp->next;
        free(temp);
        return 0;
    } 

    while (temp != NULL && temp->diskNum != disk) {
        prev = temp;
        temp = temp->next;
    }

    if (temp == NULL) return -1;   

    prev->next = temp->next;
    free(temp);
    return 0;
}

static Node* findNode(int disk) {
    Node* current = diskList;
    
    while (current != NULL) {
        if (current->diskNum == disk) {
            return current; // Key found
        }
        current = current->next;
        }
    return NULL; // Key not found
    }


// LIBDISK HELPER FUNCTIONS

// for now we can assume that we can open
// the same filename multiple times
int openDisk(char *filename, int nBytes){
    FILE* file; 
    if (nBytes == 0){    
        // open file for appending
        // tries to open file if it exists
        file = fopen(filename,"r");
        if (!file){
            return ERR_NO_FILE;
        }
        if (fclose(file) != 0){
            return ERR_FCLOSE;
        }
        // Now we know that the file exists
        file = fopen(filename,"a+");
        if (!file){
            return ERR_FOPEN;
        }
        if (insert(diskNumber, file, nBytes) == -1){
            return ERR_INS_NODE;
        }
        return diskNumber++; 
    }else if (nBytes < BLOCKSIZE){
        // less than blocksize bytes
        return ERR_NBYTES;
    }else{
        // open/create file for writing
        // need to fopen twice because w+ would truncate 
        // an already existing file
        nBytes = (nBytes % BLOCKSIZE) * nBytes;
        // tries to open file for writing if it exists 
        file = fopen(filename,"r+");
        if (!file){
            // tries to create a file
            file = fopen(filename,"w+"); 
            if (!file){ 
                return ERR_FOPEN;
            }
        }
        if (insert(diskNumber, file, nBytes) == -1){
            return ERR_INS_NODE;
        }
        return diskNumber++; 
    }
}

int closeDisk(int disk){
    Node* node = findNode(disk);
    if (node == NULL){
        return ERR_DISK_CLOSED;    
    }
    FILE* fd = node->fd;
    if (fd!= NULL){
        if(fclose(fd) != 0){
            return ERR_FCLOSE;
        } 
    }else{
        // something went wrong
       return ERR_DISK_CLOSED; 
    }
    if (deleteNode(disk) == -1){
        return ERR_DEL_NODE;
    }
    return 0; 
}

int readBlock(int disk, int bNum, void *block){
    // check if disk is open for reading
    FILE* fd;
    Node* node = findNode(disk);
    if (node == NULL){
       return ERR_DISK_CLOSED; 
    }
    // check if bNum isn't too big
    if (bNum*BLOCKSIZE > node->nBytes){
        return ERR_DISK_SIZE_EXCEEDED;
    }
    fd = node->fd;
    // load block into block
    if (!fseek(fd,bNum*BLOCKSIZE,SEEK_SET)){
        return ERR_LSEEK; 
    }
    if (!fread(block,1,BLOCKSIZE,fd)){
        return ERR_FREAD;
    } 
    return 0;
}

int writeBlock(int disk, int bNum, void *block){
    // check if disk is open
    Node* node = findNode(disk);
    FILE* fd;
    if (node == NULL){
       return ERR_DISK_CLOSED; 
    }
    // check if bNum isn't too big
    if (bNum*BLOCKSIZE > node->nBytes){
        return ERR_DISK_SIZE_EXCEEDED;
    }
    fd = node->fd;
    // write from block if possible
    if (!fseek(fd,bNum*BLOCKSIZE,SEEK_SET)){
        return ERR_LSEEK; 
    }
    if (!fwrite(block,1,BLOCKSIZE,fd)){
        return ERR_FWRITE;
    } 
    return 0;
}

int main(){
    void* write_block = "hello friends";
    void* read_block[256];
    // test the BDD
    if (openDisk("disk0.txt",255) == ERR_NBYTES){printf("1\n");}
    if (openDisk("disk0.txt",-1) == ERR_NBYTES){printf("2\n");}
    if (openDisk("disk0.txt",0) == ERR_NO_FILE){printf("3\n");}
    // successfull disk0 open and close 
    int diskNum = openDisk("disk0.txt",500);
    closeDisk(diskNum);
    // disk0 is already closed
    if (closeDisk(diskNum) == ERR_DISK_CLOSED){printf("4\n");}  
    // successfull disk1 open, write, read
    int diskNum2 = openDisk("disk1.txt",400);
    printf("%d\n", diskNum2);
    printf("h%d\n",writeBlock(diskNum2,1,write_block));
    readBlock(diskNum2,1,read_block);
    printf("%s\n",(char*)*read_block);
    return 0; 

}




