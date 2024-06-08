#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <string.h>
#include <unistd.h>
#include "libDisk.h"
#include "libTinyFS.h"
#include "TinyFS_errno.h"

#define MAGIC_NUMBER 0x44

typedef struct Node{
    fileDescriptor FD;
    char* fileName;
    struct Node* next;
}Node;

int mountedDiskNum = -1;
char* mountedDiskName = NULL;


Node* openedFiles = NULL;
fileDescriptor fdGlobal = 1;



// LIBTINY HELPER FUNCTIONS
static Node* createNode(fileDescriptor fd,char* filename) {
    Node* newNode = (Node*)malloc(sizeof(Node));
    if (newNode == NULL) {
        perror("malloc: "); 
        exit(1);
    }
    newNode->FD = fd;
    newNode->fileName = filename;
    newNode->next = NULL;
    return newNode;
}

static int insert(fileDescriptor fd,char* filename) {
    Node* newNode = createNode(fd,filename);
    Node* head = openedFiles;
    newNode->next = head;
    openedFiles = newNode; 
    return 0;
}

static int deleteNode(fileDescriptor fd){
    Node* temp = openedFiles;
    Node* prev = NULL;

    if (temp != NULL && temp->FD == fd){
        openedFiles = temp->next;
        free(temp);
        return 0;
    } 

    while (temp != NULL && temp->FD != fd) {
        prev = temp;
        temp = temp->next;
    }

    if (temp == NULL) return -1;   

    prev->next = temp->next;
    free(temp);
    return 0;
}


static Node* findNode(fileDescriptor fd) {
    Node* current = openedFiles;
    while (current != NULL) {
        if (current->FD == fd) {
            return current; // Key found
        }
        current = current->next;
        }
    return NULL; // Key not found
}

static Node* findNodeFilename(char* filename) {
    Node* current = openedFiles;
    while (current != NULL) {
        if (current->fileName == filename) {
            return current; // Key found
        }
        current = current->next;
        }
    return NULL; // Key not found
}


// libTiny function implementation

int tfs_mkfs(char* filename, int nBytes){
    char* write_block = malloc(BLOCKSIZE * sizeof(char));
    char* super_block = malloc(BLOCKSIZE * sizeof(char));
    char* root_inode = malloc(BLOCKSIZE * sizeof(char));
    char* root_directory = malloc(BLOCKSIZE * sizeof(char));
    int diskNum = openDisk(filename,nBytes);
    int err_code;
    if (diskNum < 0){
        return diskNum;
    }
    // format the file
    // set the file to 0
    memset(write_block,0x00,BLOCKSIZE);
    write_block[0] = '4';
    write_block[1] = MAGIC_NUMBER;
    int i;
    int numBlocks = ((nBytes - (nBytes % BLOCKSIZE)) / BLOCKSIZE);
    if (numBlocks < 2){
        return -1;
    }
    for (i=0; i<numBlocks; i++){
        if (i == numBlocks-1){
            write_block[2] = 0x00; //empty next free block (no more free blocks)
        }else{
            write_block[2] = i+1; //defaults the next free block to be the next available block
        }
        err_code=writeBlock(diskNum,i,write_block); 
        if (err_code < 0){
            free(write_block);
            free(super_block);
            free(root_inode);
            return err_code; 
        }
    }

    // set the superblock
    memset(super_block,0x00,BLOCKSIZE);
    super_block[0] = '0';
    super_block[1] = MAGIC_NUMBER;
    super_block[2] = 3;//pointer to next free block
    super_block[3] = 0x00;  //empty 
    super_block[4] = MAGIC_NUMBER;//magic number
    super_block[5] = 1;//pointer to root inode directory
    super_block[6] = numBlocks; //size of the disk
    
    err_code = writeBlock(diskNum,0,super_block);
    if (err_code < 0){
        free(write_block);
        free(super_block);
        free(root_inode);
        return err_code;
    }

   
    //set the root_directory_inode
    memset(root_inode,0x00,BLOCKSIZE);
    root_inode[0] = '5';
    root_inode[1] = MAGIC_NUMBER;
    root_inode[2] = 2 // address of directory file extent
    root_inode[4] = "/"; // name of directory

    root_inode[12] = 0; //size of directory 

   //set the root_directory_inode
    memset(root_directory,0x00,BLOCKSIZE);
    root_directory[0] = '3';
    root_directory[1] = MAGIC_NUMBER;
    root_directory[2] = 0 // address of directory file extent
 
    err_code = writeBlock(diskNum,1,root_inode);
    if (err_code < 0){
        free(write_block);
        free(super_block);
        free(root_inode);
        free(root_directory);
        return err_code;
    }

    err_code = writeBlock(diskNum,2,root_directory);
    if (err_code < 0){
        free(write_block);
        free(super_block);
        free(root_inode);
        free(root_directory);
        return err_code;
    }

  
    // the disk is set up
    // lets close it for now
    err_code = closeDisk(diskNum);
    if (err_code < 0){
        free(write_block);
        free(super_block);
        free(root_inode);
        return err_code;
    }

    free(write_block);
    free(super_block);
    free(root_inode);
    return SUCCESS;
}

int tfs_mount(char* diskname){
    // check if file is a disk
    // check if a disk is already mounted
    // check if we have the right format
    // 1st block is superblock
    // all blocks have the magic number
    
    if (mountedDiskNum != -1){
        return ERR_DISK_MOUNTED;
    }    
    char* read_block = malloc(BLOCKSIZE * sizeof(char));

    int diskNum; 
    diskNum = openDisk(diskname, 0); 
    if (diskNum < 0){
        return diskNum;
    }
    //lets read the superblock
    int err_code;
    err_code = readBlock(diskNum,0,read_block);
    if (err_code < 0){
        free(read_block);
        return err_code;
    }
    if (read_block[1] != MAGIC_NUMBER){
        free(read_block);
        return ERR_INVALID_TINYFS; 
    }
    //validate all the blocks
    int numBlocks = read_block[6];
    printf("numblocks: %d\n", numBlocks);
    int i;
    for (i=1;i<numBlocks;i++){
        err_code = readBlock(diskNum,i,read_block);
        if (err_code < 0){
            free(read_block);
            return err_code;
        }
        
        if (read_block[1] != MAGIC_NUMBER){
            free(read_block);
            return ERR_INVALID_TINYFS; 
        }
    }
    // ok now we know the disk size
    // reopen disk for writing
    err_code = closeDisk(diskNum);
    if (err_code < 0){
        free(read_block);
        return err_code;
    }
    
    free(read_block);
    openDisk(diskname, numBlocks*BLOCKSIZE);

    mountedDiskNum = diskNum;
    mountedDiskName = diskname;
    return SUCCESS;

}

int tfs_closeFile(fileDescriptor FD){
    // remove the node from our open list
    int err_code = deleteNode(FD);
    if (err_code < 0){
        return err_code;
    }
    return SUCCESS;
}


int tfs_unmount(void){
    // lets unmount the currently mounted file
    //close all the open files
    Node* cur = openedFiles;
    Node* next = NULL;
    int err_code;
    while (cur != NULL){
        next = cur->next;
        err_code = tfs_closeFile(cur->FD);
        if (err_code < 0){
            return err_code;
        }
        cur = next; 
    } 

    mountedDiskNum = -1;
    mountedDiskName = NULL;
    return SUCCESS;     

}


int addNewFile(char* name){

    // inode
    char* inode_block = malloc(BLOCKSIZE * sizeof(char));
    char* read_block = malloc(BLOCKSIZE * sizeof(char));

    memset(inode_block,0x00,BLOCKSIZE);
    inode_block[0] = '2';
    inode_block[1] = MAGIC_NUMBER;
    inode_block[2] = 0x00;
    inode_block[3] = 0x00;  //empty   
    int i;
    if (sizeof(name) > 8){
        return -1;
    }
    for (i=0;i<8;i++){
        if (name[i] == '\0'){
            break;
        }
        inode_block[4+i] = name[i];
    }   
    inode_block[13] = 0; 

    // find free block
    int err_code;
    err_code = readBlock(mountedDiskNum,0,read_block);
    if (err_code < 0){
        free(inode_block);
        free(read_block);
        return err_code;
    }
    int freeBlock = read_block[2]; // next free block
    printf("freeblock %d\n",freeBlock);

    // check if disk is full
    if (freeBlock < 1){
        free(inode_block);
        free(read_block);
        return -1;
    }
    // get the next free block to update in superblock
    err_code = readBlock(mountedDiskNum,freeBlock,read_block);
    if (err_code < 0){
        free(inode_block);
        free(read_block);
        return err_code;
    }
    int nextBlock = read_block[2];
    
    // update superblock  
    err_code = readBlock(mountedDiskNum,0,read_block);
    if (err_code < 0){
        free(inode_block);
        free(read_block);
        return err_code;
    }
    read_block[2] = nextBlock;
    int root_inode = read_block[5];
    err_code = writeBlock(mountedDiskNum,0,read_block);
    if (err_code < 0){
        free(inode_block);
        free(read_block);
        return err_code;
    }

    //add the inode block number to the root inode directory
    err_code = readBlock(mountedDiskNum,root_inode,read_block);
    if (err_code < 0){
        free(inode_block);
        free(read_block);
        return err_code;
    }
   
    i=4;
    while(read_block[i] != 0x00){
        i++;
    }
    read_block[i] = freeBlock;

    err_code = writeBlock(mountedDiskNum,root_inode,read_block);
    if (err_code < 0){
        free(inode_block);
        free(read_block);
        return err_code;
    }
 
    // add the inode block
    err_code = writeBlock(mountedDiskNum,freeBlock,inode_block);
    if (err_code < 0){
        free(inode_block);
        free(read_block);
        return err_code;
    }
    
    // add the fdGlobal to the linked list 
    err_code = insert(fdGlobal, name);
    if (err_code < 0){
        free(inode_block);
        free(read_block);
        return err_code;
    }
    free(inode_block);
    free(read_block);
    return fdGlobal++;
}



fileDescriptor tfs_openFile(char* name){
    // open the file
    // search thru file system
    // if not there, create
    // if its there
    // save the FD into the linked list
    if (mountedDiskNum == -1){
       return -1; 
    } 
    // check our table
    Node* node = findNodeFilename(name);
    if (node == NULL){
        // create it
        fileDescriptor newFd = addNewFile(name);
        if (newFd < 0){
            return newFd; 
        }
        return newFd; 
    }else{
        return node->FD;
    }

}   

int tfs_writeFile(fileDescriptor FD, char* buffer, int size){
    Node* node = findNode(FD); 
    char* block = (char*)malloc(BLOCKSIZE * sizeof(char));
    char* read_block = (char*)malloc(BLOCKSIZE * sizeof(char));
    if (node == NULL){
        return -1; 
    }
    char* filename = node->fileName;
    int err_code;
    // search the root inode for the filename
    err_code = readBlock(mountedDiskNum,0,block);
    if (err_code < 0){
        free(block);
        free(read_block);
        return err_code;
    }
    int root_inode = block[5];
    err_code = readBlock(mountedDiskNum,root_inode,block);
    if (err_code < 0){
        free(block);
        free(read_block);
        return err_code;
    }

    int i=4;
    int flag;
    int j;
    while(i<BLOCKSIZE && block[i] != 0x00){
        err_code = readBlock(mountedDiskNum,block[i],read_block);
        if (err_code < 0){
            free(block);
            free(read_block);
            return err_code;
        }
        flag = 1;
        for(j=0;j<8;j++){
            if (read_block[4+j] == '\0'){
                break;
            }
            if(read_block[4+j] != filename[j]){
                flag = 0;
                break; // we didn't get the right inode
            } 
        }
        if (flag){
            // we've found our file
            break;
        } 
        i++;
    }
    if (flag){
        err_code = readBlock(mountedDiskNum,0,block);
        if (err_code < 0){
            free(block);
            free(read_block);
            return err_code;
        }
        int free_block = block[2];
        //lets write to this inodes file extent
        read_block[2] = free_block;
        read_block[13] = size;
        err_code = writeBlock(mountedDiskNum,i,read_block);
        if (err_code < 0){
            free(block);
            free(read_block);
            return err_code;
        }
        
        err_code = readBlock(mountedDiskNum,free_block,block);
        if (err_code < 0){
            free(block);
            free(read_block);
            return err_code;
        }
          
        // now the rest of the buffer can be
        for (i=0; i<size; i++){
            if (i+4 % BLOCKSIZE == 0){
                block[0] = '3';
                block[1] = MAGIC_NUMBER;
                //write this block and set up the next one
                err_code = writeBlock(mountedDiskNum,free_block,block);
                if (err_code < 0){
                    free(block);
                    free(read_block);
                    return err_code;
                }
               
                free_block = block[2];
                err_code = readBlock(mountedDiskNum,free_block,block);
                if (err_code < 0){
                    free(block);
                    free(read_block);
                    return err_code;
                }
            }
            block[i+4 % BLOCKSIZE] = buffer[i];
        }

        // set the next free node in the superblock
        free_block = block[2];
        err_code = readBlock(mountedDiskNum,0,block);
        if (err_code < 0){
            free(block);
            free(read_block);
            return err_code;
        }
        block[2] = free_block;
        err_code = writeBlock(mountedDiskNum,0,block);
        if (err_code < 0){
            free(block);
            free(read_block);
            return err_code;
        }

        return SUCCESS;
    } 


}

int tfs_deleteFile(fileDescriptor FD){

}

int tfs_readByte(fileDescriptor FD, char* buffer){

}

int tfs_seek(fileDescriptor FD, int offset){

}

int main(){
    printf("%d\n",tfs_mkfs("disk0.dsk",2562));
    tfs_mount("disk0.dsk");
    fileDescriptor fd = tfs_openFile("ritvik");
    char* message = "hi my name is Joel";
    int err = tfs_writeFile(fd,message,sizeof(message));
    printf("%d\n",err);
    tfs_unmount();
    return 0;
}
