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


// this overwrites any existing files
// with the same name
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
    root_inode[2] = 2; // address of directory file extent
    root_inode[4] = '/'; // name of directory

    root_inode[12] = 0; //size of directory 

   //set the root_directory_inode
    memset(root_directory,0x00,BLOCKSIZE);
    root_directory[0] = '3';
    root_directory[1] = MAGIC_NUMBER;
    root_directory[2] = 0; // address of directory file extent
 
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

static char* substring(char* string, int start, int end){
    // Copy the substring
    int len = end-start;
    char* sub = (char*) malloc(sizeof(char) * (len+1));
    strncpy(sub, string + start, len);
    // Null-terminate the substring
    sub[len] = '\0';
    return sub;
}

static int checkDirectory(char* name,char* buffer){
    int i;
    char* entry;
    int len;
    for (i=4;i<BLOCKSIZE;i+=9){
        entry = substring(buffer,i,i+8);
        len = strlen(name);
        if (strncmp(name,entry,2) == 0){
            return buffer[i+8]; // returns the inode
        }
    }
    return 0; //returns 0 if not in directory
}


// return values:
// if < 0 --> can't find a directory or other error
// if = 0 --> can't find the filename
// if > 1 --> this is the inode associated with file
static int searchForFile(char* path, char* root_buffer, char* filename){
    int anchor = 1;
    int inode;
    int i;
    int cur_directory;

    if (path[0] != '/'){
        return -20;
    }
    for (i=1;i<strlen(path);i++){
        if (path[i] == '/'){
            //directory name 
            char* dirName = substring(path,anchor,i);
            // read the superblock, find the root directory inode
            inode = checkDirectory(dirName,root_buffer);
            free(dirName);
            if (inode != 0){
                readBlock(mountedDiskNum,inode,root_buffer);
                cur_directory = root_buffer[2];
                readBlock(mountedDiskNum,cur_directory,root_buffer);
                // now read_block has the contents of the directory
            }else{
                return -30;
            }
            anchor = i+1;
        }   
    }
    char* temp = substring(path,anchor,strlen(path));
    strncpy(filename,temp,8);
    free(temp);
    if (sizeof(filename) > 8){
        return -1; //  not a possible filename
    }
    inode = checkDirectory(filename,root_buffer);
    if (inode != 0){
        return inode;
    }else{
        return 0;
    }
}

static int addFileToBuffer(char* name, char* buffer, int inode){
    int i;
    int j;
    for (i=4; i<BLOCKSIZE;i+=9){
        if (buffer[i] == '\0'){
            for (j=0;j<8;j++){
                if (name[j] == '\0'){
                    buffer[i+j] = inode;
                    return 0;
                }
                buffer[i+j] = name[j];  
            }
            buffer[i+j] = inode; 
            return 0;
        }
    }
    // we can't find enough space in current dictionary
    // TO-DO: extend the file extent
    printf("oopsies");
    return -1;
}

int addNewFile(char* name){

    // inode
    char* inode_block = malloc(BLOCKSIZE * sizeof(char));
    char* read_block = malloc(BLOCKSIZE * sizeof(char));
    int err_code;

    memset(inode_block,0x00,BLOCKSIZE);
    inode_block[0] = '2';
    inode_block[1] = MAGIC_NUMBER;
    inode_block[2] = 0;
    inode_block[3] = 0x00;  //empty   
    int i;

    // read from superblock
    readBlock(mountedDiskNum,0,read_block);
    int freeBlock = read_block[2]; // next free block
    // check if disk is full
    if (freeBlock == 0){
        free(inode_block);
        free(read_block);
        return -1;
    }
    
    int root_inode = read_block[5];
    readBlock(mountedDiskNum,root_inode,read_block);
    int cur_directory = read_block[2];
    readBlock(mountedDiskNum,cur_directory,read_block);

    char* filename = (char *)malloc(9*sizeof(char));
    if (searchForFile(name,read_block,filename) < 0){
        return -30;
    }

    printf("my filename: %s\n",filename);

    // for directory implementation we are adding the filename
    // the current directory is in read_block
    addFileToBuffer(filename,read_block,freeBlock);
    writeBlock(mountedDiskNum,cur_directory,read_block);
 
    for (i=0;i<8;i++){
        if (filename[i] == '\0'){
            break;
        }
        inode_block[4+i] = filename[i];
    }   
    inode_block[12] = 0; // size of the new file

    // get the next free block to update in superblock
    err_code = readBlock(mountedDiskNum,freeBlock,read_block);
    if (err_code < 0){
        free(inode_block);
        free(read_block);
        return err_code;
    }
    int nextBlock = read_block[2];
    
    // add the inode block
    err_code = writeBlock(mountedDiskNum,freeBlock,inode_block);
    if (err_code < 0){
        free(inode_block);
        free(read_block);
        return err_code;
    }
    
    // update superblock  
    err_code = readBlock(mountedDiskNum,0,read_block);
    if (err_code < 0){
        free(inode_block);
        free(read_block);
        return err_code;
    }
    read_block[2] = nextBlock;
    err_code = writeBlock(mountedDiskNum,0,read_block);
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
    int err_code;
    char* read_block = (char*)malloc(BLOCKSIZE * sizeof(char));
    if (mountedDiskNum == -1){
       return -1; 
    } 
    // check our table
    Node* node = findNodeFilename(name);
    if (node == NULL){
        // create it
        char* filename = malloc(9 * sizeof(char));
        readBlock(mountedDiskNum,0,read_block);
        int root_inode = read_block[5];
        readBlock(mountedDiskNum,root_inode,read_block);
        int cur_directory = read_block[2];
        readBlock(mountedDiskNum,cur_directory,read_block);
        int inode;
        inode = searchForFile(name,read_block,filename);
        if (inode < 0){
            // invalid path name or other error
            return inode;
        }else if (inode == 0){
            // add a new file
            // note that read_block should contain contents of 
            // the last subdirectory
            fileDescriptor newFd = addNewFile(name);
            printf("%d\n",newFd);
            if (newFd < 0){
                return newFd; 
            }
            free(read_block);
            return newFd; 
        }else{
            // add to openedfilesi
            printf("on disk\n");
            err_code = insert(fdGlobal, name);
            if (err_code < 0){
                free(read_block);
                return err_code;
            }
            free(read_block);
            return fdGlobal++;
        }
    }else{
        free(read_block);
        printf("in list\n");
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
    fileDescriptor fd = tfs_openFile("/ritvik");
    fileDescriptor fd2 = tfs_openFile("/joel");
    fileDescriptor fd3 = tfs_openFile("/joel");
    fileDescriptor fd4 = tfs_openFile("/ty");
    char* message = "hi my name is Joel";
    int err = tfs_writeFile(fd,message,sizeof(message));
    printf("%d\n",err);
    tfs_unmount();
    return 0;
}
