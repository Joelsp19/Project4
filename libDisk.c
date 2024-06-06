#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <string.h>
#include <unistd.h>
#include "TinyFS_errno.h"
#include "libDisk.h"

#define BLOCKSIZE 256

#define READ_MODE 0
#define WRITE_MODE 1
#define OVERWRITE_MODE 2

typedef struct Node{
    int diskNum;
    char* filename;
    FILE* fd;
    int nBytes;
    int mode;
    struct Node* next;
}Node;

// GLOBAL VARIABLES

// global unique disk number
int diskNumber = 0;
// we need a way to store the fd, and nbytes
// use a global linked list implementation
Node* diskList = NULL;


// DISKLIST HELPER FUNCTIONS
static Node* createNode(int diskNum, char* filename, int nBytes,int mode) {
    Node* newNode = (Node*)malloc(sizeof(Node));
    if (newNode == NULL) {
        perror("malloc: "); 
        exit(1);
    }
    newNode->diskNum = diskNum;
    newNode->nBytes = nBytes;
    newNode->filename = filename;
    newNode->mode = mode;
    newNode->next = NULL;
    return newNode;
}

static int insert(int diskNum, char* filename, int nBytes,int mode) {
    Node* newNode = createNode(diskNum, filename, nBytes,mode);
    Node* head = diskList;
    newNode->next = head;
    diskList = newNode; 
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
        // Now we know that the file exists
        if (insert(diskNumber, filename, nBytes,READ_MODE) == -1){
            return ERR_INS_NODE;
        }
        fclose(file);
        return diskNumber; 
    }else if (nBytes < BLOCKSIZE){
        // less than blocksize bytes
        return ERR_NBYTES;
    }else{
        // open/create file for writing
        // need to fopen twice because w+ would truncate 
        // an already existing file
        nBytes = (nBytes - (nBytes%BLOCKSIZE));
        // tries to open file for writing if it exists 
        file = fopen(filename,"r+");
        int mode = OVERWRITE_MODE;
        if (!file){
            // tries to create a file
            file = fopen(filename,"w+"); 
            if (!file){ 
                return ERR_FOPEN;
            }
            mode = WRITE_MODE;
        }
        fclose(file);
        if (insert(diskNumber, filename, nBytes,mode) == -1){
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
    
    if (node->nBytes != 0){
        if (bNum*BLOCKSIZE > node->nBytes){
            return ERR_DISK_SIZE_EXCEEDED;
        }
    }

    char* file = node->filename;
    if (!file){
        return ERR_NO_FILE;
    }
    fd = fopen(node->filename,"r");
    
    // load block into block
    if (fseek(fd,bNum*BLOCKSIZE,SEEK_SET) != 0){
        return ERR_FSEEK; 
    }
    fread(block,1,BLOCKSIZE,fd);
    if (feof(fd) != 0 || ferror(fd)!=0){
        return ERR_FREAD;
    }
    fclose(fd);
    return 0;
}

int writeBlock(int disk, int bNum, void *block){
    // check if disk is open
    Node* node = findNode(disk);
    FILE* fd;

    if (node->mode != WRITE_MODE && node->mode != OVERWRITE_MODE){
       return ERR_NO_WRITE; 
    }    

    if (node == NULL){
       return ERR_DISK_CLOSED; 
    }
    // check if bNum isn't too big
    if (bNum*BLOCKSIZE > node->nBytes){
        printf("%d\n",node->nBytes);
        printf("%d\n",bNum*BLOCKSIZE);
        return ERR_DISK_SIZE_EXCEEDED;
    }
    char* file = node->filename;
    if (!file){
        return ERR_NO_FILE;
    }

    if (node->mode == WRITE_MODE){
        fd = fopen(file,"w+");
    }else{
        fd = fopen(file,"r+");
    }

    // write from block if possible
    if (fseek(fd,bNum*BLOCKSIZE,SEEK_SET) != 0){
        return ERR_FSEEK; 
    }
    fwrite(block,1,BLOCKSIZE,fd);
    if (feof(fd) != 0 || ferror(fd)!=0){
        return ERR_FWRITE;
    }
    return 0;
}

/*int main(){
    void* write_block[256];
    void* read_block[256];
    char* msg = "hello friends";
    
    memset(write_block,' ',sizeof(write_block));
    strncpy((char*)write_block, msg, strlen(msg));
    
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
    writeBlock(diskNum2,1,write_block);
    readBlock(diskNum2,1,read_block);
    closeDisk(diskNum2);
    printf("%s\n",(char*)read_block);
    // open two disks that have already been created for reading
    int diskNum3 = openDisk("disk1.txt",0);
    int diskNum4 = openDisk("disk0.txt",0);
    // try to overwrite the file we wrote to
    msg = "overwrite";
    strncpy((char *)write_block, msg, strlen(msg));
    if (writeBlock(diskNum4,1,write_block) == -12){printf("5\n");}

    return 0; 

}*/




