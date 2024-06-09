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

static int modifyFilename(fileDescriptor fd, char* newFileName){
    Node* n = findNode(fd);
    if (n == NULL){
        return -1;
    }else{
        n->fileName = newFileName;
    } 
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

    //get free list
    char* read_block_fl = malloc(BLOCKSIZE * sizeof(char));
    int MAX_BLOCKS = read_block[6];
    int* free_list = malloc(MAX_BLOCKS * sizeof(int));

    //setting up list of free blocks
    int k;
    for(k = 0; k < MAX_BLOCKS; k++){
        free_list[k] = -1;
    }
    
    //All the stuff for the first free block
    k = 0;
    int next_free_block = read_block[2]; //inital next free block 
    //printf("first block free %d\n", next_free_block);
    free_list[k] = next_free_block; // adds the next free block to the list
    k++;

    while(next_free_block > 0){
        //printf("here: %d\n", next_free_block);
        err_code = readBlock(diskNum, next_free_block, read_block_fl); // reads the next free block
        if(err_code < 0 || read_block_fl[2] == 0){
            free(read_block_fl);
            break;
        }
        //printf("read block free: %d\n", read_block_fl[2]);
        next_free_block = read_block_fl[2]; // sets new free block 
        free_list[k] = next_free_block; // adds the next free block to the list
        k++;
    }

    int j;
    for(j = 0; j <  MAX_BLOCKS; j++){
        printf("freeblock: %d\n", free_list[j]);
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

        if(read_block[0] == '4'){
            // if node is free but not in free list
            int found = 0;
            int j;
            for(j = 0; j <  MAX_BLOCKS; j++){
                if (free_list[j] == i){
                    found = 1;
                } 
            } 
            if(found != 1){
                printf("free node not in list \n");
                return ERR_INVALID_TINYFS; 
            }
        } else {
            // checks if a non free node is in the free list
            int found = 0;
            int j;
            for(j = 0; j < MAX_BLOCKS; j++){
                if(free_list[j] == i){
                    found = 1;
                }
            }

            if(found){
                printf("Non free node found in list \n");
                return ERR_INVALID_TINYFS;
            }
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
        if (strncmp(name,entry,len) == 0){
            return buffer[i+8]; // returns the inode
        }
    }
    return 0; //returns 0 if not in directory
}


static int modifyFileFromDirectory(char* name,char* buffer,char* newContent){
    int i;
    int j;
    char* entry;
    int len;
    int flag = 0;
    for (i=4;i<BLOCKSIZE;i+=9){
        entry = substring(buffer,i,i+8);
        len = strlen(name);
        printf("entry %s\n",entry);
        if (strncmp(name,entry,len) == 0){
            if (newContent == NULL){
                for(j=0;j<9;j++){
                    buffer[i+j] = 0;
                }
            }else{
                for(j=0;j<8;j++){
                    if (newContent[j] == '\0'){
                        flag = 1;
                    }
                    if (!flag){
                        buffer[i+j] = newContent[j];
                    }else{
                        buffer[i+j] = 0;
                    }
                }
            }   
            return 1; // returns true if worked
        }
    }
    return -1; //returns -1 if not in directory
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
            printf("dirName %s\n", dirName);
            // read the superblock, find the root directory inode
            inode = checkDirectory(dirName,root_buffer);
            printf("inode %d\n", inode);
            free(dirName);
            if (inode != 0){
                readBlock(mountedDiskNum,inode,root_buffer);
                cur_directory = root_buffer[2];
                printf("cur directory %d\n", inode);
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


static int getRecentDirectory(char* path, char* root_buffer){
    int anchor = 1;
    int inode;
    int i;
    int cur_directory = 0;

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
    return cur_directory;
}



static int addFileToBuffer(char* name, char* buffer, int inode){
    int i;
    int j;
    for (i=4; i<BLOCKSIZE;i+=9){
        if (buffer[i] == '\0'){
            for (j=0;j<8;j++){
                if (name[j] == '\0'){
                    break;
                }
                buffer[i+j] = name[j];  
            }
            buffer[i+8] = inode; 
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
    int subDir;
    char* filename = (char *)malloc(9*sizeof(char));
    int inode = searchForFile(name,read_block,filename);
    if(inode < 0){
        return -30;
    }else if (inode == 0){
        // assume we add to the root directory
        subDir = cur_directory;
    }else{
        readBlock(mountedDiskNum, inode,read_block);
        subDir = read_block[2];
    }   
    printf("my filename: %s\n",filename);

    // for directory implementation we are adding the filename
    // the current directory is in read_block
    readBlock(mountedDiskNum, subDir, read_block);    

    addFileToBuffer(filename,read_block,freeBlock);
    writeBlock(mountedDiskNum,subDir,read_block);
 
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
    int i;
    int free_block;

    if (node == NULL){
        return -1; 
    }
    char* path = node->fileName;
    int err_code;
    
    // we want to search the directory for the filename
    
    char* filename = malloc(9 * sizeof(char));
    readBlock(mountedDiskNum,0,read_block);
    int root_inode = read_block[5];
    readBlock(mountedDiskNum,root_inode,read_block);
    int cur_directory = read_block[2];
    readBlock(mountedDiskNum,cur_directory,read_block);
    
    int inode;
    inode = searchForFile(path,read_block,filename);
    if (inode < 0){
        return inode; // invalid diretory or other 
    }else if (inode == 0){
        return -4; //cant find filename
    }
    printf("%d\n",inode);
    readBlock(mountedDiskNum,inode,read_block);
    int file_extent = read_block[2];
    if (file_extent == 0){
        // get the next free block
        err_code = readBlock(mountedDiskNum,0,block);
        if (err_code < 0){
            free(block);
            free(read_block);
            return err_code;
        }
        free_block = block[2];
        printf("first free block %d\n",free_block);
        //lets write to this inodes next
        read_block[2] = free_block;
        read_block[12] = size % (BLOCKSIZE-4); // size of last block
        if (read_block[12] == 0){
            read_block[13] = size / (BLOCKSIZE-4); // number of bytes
        }else{
            read_block[13] = size / (BLOCKSIZE-4) + 1; // number of bytes
        }
        printf("double check 12 %d\n",read_block[12]);
        printf("double check 13 %d\n",read_block[13]);
        read_block[14] = 0; // cur byte file pointer
        read_block[15] = 0; // cur block file pointer
        err_code = writeBlock(mountedDiskNum,inode,read_block);
        if (err_code < 0){
            free(block);
            free(read_block);
            return err_code;
        }
        readBlock(mountedDiskNum,free_block,read_block);
    }else{
        // delete existing blocks
        // update the inode contents to be correct
        readBlock(mountedDiskNum,inode,read_block);
        free_block = block[2];
        readBlock(mountedDiskNum,free_block,read_block);
    }
    // at this point, read_block contain first file extent content

    int next_block = read_block[2];

    // now the rest of the buffer can be
    int j=1;
    for (i=0; i<size; i++){
        if ((i+4*j) % BLOCKSIZE == 0){
            read_block[0] = '3';
            read_block[1] = MAGIC_NUMBER;
            //write this block and set up the next one
            err_code = writeBlock(mountedDiskNum,free_block,read_block);
            if (err_code < 0){
                free(block);
                free(read_block);
                return err_code;
            }
            
            free_block = next_block;
            printf("free block %d\n", free_block);
            err_code = readBlock(mountedDiskNum,free_block,read_block);
            if (err_code < 0){
                free(block);
                free(read_block);
                return err_code;
            }
            next_block = read_block[2];
            j+=1;
        }
        read_block[(i+4*j) % BLOCKSIZE] = buffer[i];
    }

    // write the last block
    block[0] = '3';
    block[1] = MAGIC_NUMBER;
    //write this block and set up the next one
    err_code = writeBlock(mountedDiskNum,free_block,read_block);
    if (err_code < 0){
        free(block);
        free(read_block);
        return err_code;
    }
    
    free_block = read_block[2];
    
    // set the next free node in the superblock
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

int tfs_deleteFile(fileDescriptor FD)
{
    char* read_block = (char*)malloc(sizeof(char) * BLOCKSIZE);
    char* write_block = (char*)malloc(sizeof(char) * BLOCKSIZE);
    char* temp_fil = (char*)malloc(sizeof(char) * 9);

    int err_code;
    //read from superblock, get curr directory file
    readBlock(mountedDiskNum,0,read_block);
    int root_inode = read_block[5];
    readBlock(mountedDiskNum,root_inode,read_block);
    int cur_directory = read_block[2];
    printf("cur directory %d\n", cur_directory);
    readBlock(mountedDiskNum,cur_directory,read_block);

    //find inode block based on file descriptor
    Node* fil = findNode(FD);
    if (fil == NULL){
        return -2;
    }
    char* path = fil->fileName;
    int inode = searchForFile(path, read_block, temp_fil);

    //read_block contains inode block for file to delete
    readBlock(mountedDiskNum, inode, read_block);

    int file_extent = read_block[2];
    //will 0 out every node, and make it into a free block
    memset(write_block, 0x00, BLOCKSIZE);
    write_block[0] = '4';
    write_block[1] = MAGIC_NUMBER;
    write_block[2] = file_extent;
    writeBlock(mountedDiskNum, inode, write_block);
    readBlock(mountedDiskNum, file_extent, read_block);
    int tracker = read_block[0];

    int prev_extent = inode;
    while (tracker == '3')
    {
        //keep same link
        write_block[2] = read_block[2];
        //write in same file extent, the new buffer
        writeBlock(mountedDiskNum, file_extent, write_block);
        //update to next file_extent file
        prev_extent = file_extent;
        file_extent = read_block[2];
        readBlock(mountedDiskNum, file_extent, read_block);
        tracker = read_block[0];
    }

    //read superblock again and update it to old inode block
    readBlock(mountedDiskNum, 0, read_block);
    int new_free = read_block[2];
    read_block[2] = inode;
    writeBlock(mountedDiskNum, 0, read_block);
    
    //wewrite the last deleted file to point to what superblock was pointing to
    readBlock(mountedDiskNum, prev_extent, read_block);
    read_block[2] = new_free;
    writeBlock(mountedDiskNum, prev_extent, read_block);

    //HAVE TO DELETE IT FROM LAST DIRECTORY STILL
       
    readBlock(mountedDiskNum,cur_directory,read_block);
    int dir_inode = getRecentDirectory(path,read_block);
    if (dir_inode <= 0){
        dir_inode = cur_directory;// assume its the root 
    }
 
    // we know have the most recent directory in read_block 
    readBlock(mountedDiskNum,dir_inode,read_block);
    
    printf("%s\n",temp_fil);
    err_code = modifyFileFromDirectory(temp_fil,read_block,NULL);
    printf("%d\n", err_code);
    if (err_code < 0){
        free(read_block);
        free(write_block);
        free(temp_fil);
        return -1;
    }
    writeBlock(mountedDiskNum,dir_inode,read_block); 
 
    free(read_block);
    free(write_block);
    free(temp_fil);
    return 0;
}

int tfs_readdir()
{
    int i, inode;
    char* read_block = (char*)malloc(sizeof(char) * BLOCKSIZE);
    char* temp_inode_reader = (char*)malloc(sizeof(char) * BLOCKSIZE);
    char* filename = (char*)malloc(sizeof(char) * 9);

    //read from superblock, get curr directory file
    readBlock(mountedDiskNum,0,read_block);
    int root_inode = read_block[5];
    readBlock(mountedDiskNum,root_inode,read_block);
    int cur_directory = read_block[2];
    readBlock(mountedDiskNum,cur_directory,read_block);

    for (i = 4; i < BLOCKSIZE; i += 9)
    {
        filename = substring(read_block, i, i + 8);
        inode = read_block[i + 9];
        readBlock(mountedDiskNum, inode, temp_inode_reader);
        if(temp_inode_reader[0] == '2')
        {
            if (filename[0] != '\0'){
                printf("here");
                printf("%s\n", filename);
            }
        }
        else
        {
            //WILL DO SOMETHING DIFFERENT FOR DIRECTORIES
            if (filename[0] != '\0'){
                printf("%s\n", filename);
            }
        }
    }
    free(read_block);
    free(temp_inode_reader);
    free(filename);
    return 1;
}

int tfs_rename(fileDescriptor FD, char* newName)
{
    if (strlen(newName) > 8)
    {
        return -1;
    }
    int i;
    int err_code;
    char* read_block = (char*)malloc(sizeof(char) * BLOCKSIZE);
    char* temp_fil = (char*)malloc(sizeof(char) * 9);

    //read from superblock, get curr directory file
    readBlock(mountedDiskNum,0,read_block);
    int root_inode = read_block[5];
    readBlock(mountedDiskNum,root_inode,read_block);
    int cur_directory = read_block[2];
    readBlock(mountedDiskNum,cur_directory,read_block);

     //find inode block based on file descriptor
    Node* fil = findNode(FD);
    if (fil == NULL)
    {
        return -1;
    }

    
    char* path = fil->fileName;
    int len = strlen(path);

    // lets change the global node block to reflect our new name
    // only need to change the last part of the file
    int k;
    for (k=strlen(path);k>=0;k--){
        if (path[k] == '/'){
            break;
        }
    }
    char* newPath = (char*)malloc(strlen(path) * sizeof(char));
    strncpy(newPath,path,len);   
    for (i=0;i<strlen(newName);i++){
        newPath[i+k+1] = newName[i]; 
    }
    newPath[i+k+1] = '\0';
    
    printf("newpath: %s\n", newPath); 
    modifyFilename(FD, newPath);
    free(newPath);
 
    //retrieve inode block and change the name within the inode block
    int inode = searchForFile(path, read_block, temp_fil);
    if (inode < 0){
        return inode; // invalid diretory or other 
    }else if (inode == 0){
        return -20;
    }
    
    readBlock(mountedDiskNum, inode, read_block);
    for (i = 0; i < 8; i++)
    {
        if (newName[i] == '\0')
        {
            break;
        }
        read_block[4+i] = newName[i];
    }
   
    //if name is less than 8, put null characs at the end
    while (i < 8)
    {
        read_block[4+i] = '\0';
        i = i + 1;
    }
    
    writeBlock(mountedDiskNum, inode, read_block);
  
    //HAVE TO STILL GO TO LAST DIRECTORY AND CHANGE THE NAME IN THERE
    
    int dir_inode = getRecentDirectory(path,read_block);
    if (dir_inode <= 0){
        dir_inode = cur_directory;// assume its the root 
    }
    printf("dir_inode: %d\n",dir_inode);
 
    // we know have the most recent directory in read_block 
    readBlock(mountedDiskNum,dir_inode,read_block);
    err_code = modifyFileFromDirectory(temp_fil,read_block,newName);
    if (err_code < 0){
        free(read_block);
        free(temp_fil);
        return -1;
    }
    writeBlock(mountedDiskNum,dir_inode,read_block);

    free(read_block);
    free(temp_fil);
    return 1;
}

// returns the inode of this 
int tfs_createDir(char* dirPath){
    // we want to create a directory if it doesn't exist already
    char* dirName = (char*)malloc(sizeof(char) * 9);
    char* read_block = (char*)malloc(sizeof(char) * BLOCKSIZE);

    int err_code;
    //read from superblock, get curr directory file
    readBlock(mountedDiskNum,0,read_block);
    int root_inode = read_block[5];
    int free_block = read_block[2];
    readBlock(mountedDiskNum,root_inode,read_block);
    int cur_directory = read_block[2];
    readBlock(mountedDiskNum,cur_directory,read_block);


    int inode = searchForFile(dirPath,read_block,dirName);
    if (inode < 0){
        return inode; // invalid diretory or other 
    }else if (inode == 0){
         //create directory
    }else{
        return inode; // the inode of this directory
    }

    int dir_inode = getRecentDirectory(dirPath,read_block);
    if (dir_inode <= 0){
        dir_inode = cur_directory;// assume its the root 
    }
 
    // we know have the most recent directory in read_block 
    readBlock(mountedDiskNum,dir_inode,read_block);
    // we want to write the filename + inode number to this block
    // find an empty spot and then write to the directory
    addFileToBuffer(dirName,read_block,free_block);
    writeBlock(mountedDiskNum,dir_inode,read_block);

    // directory inode content
    readBlock(mountedDiskNum,free_block,read_block);
    read_block[0] = '5';
    read_block[1] = MAGIC_NUMBER;
    int next_free = read_block[2] ; // defaults to next free_block - use for the directory content
    read_block[3] = 0;
   
    int i; 
    for (i=0;i<8;i++){
        if (dirName[i] == '\0'){
            break;
        }
        read_block[4+i] = dirName[i];
    }   
    writeBlock(mountedDiskNum,free_block,read_block);

    // directory content
    free_block = next_free;
    readBlock(mountedDiskNum,free_block,read_block);
    read_block[0] = '3';
    read_block[1] = MAGIC_NUMBER;
    next_free = read_block[2] ; // defaults to next free_block - save to superblock
    read_block[3] = 0;
    writeBlock(mountedDiskNum,free_block,read_block);
     
    // write to the superblock
    readBlock(mountedDiskNum,0,read_block);
    read_block[2] = next_free;
    writeBlock(mountedDiskNum,0,read_block);
    return 0;
}

int tfs_removeDir(char *dirname)
{
    int i, inode;
    char* read_block = (char*)malloc(sizeof(char) * BLOCKSIZE);
    char* buffer = (char*)malloc(sizeof(char) * BLOCKSIZE);
    char* temp_fil = (char*)malloc(sizeof(char) * 9);

    //read from superblock, get curr directory file
    readBlock(mountedDiskNum,0,read_block);
    int root_inode = read_block[5];
    readBlock(mountedDiskNum,root_inode,read_block);
    int cur_directory = read_block[2];
    readBlock(mountedDiskNum,cur_directory,read_block);

    //search for directory in root direc and get inode num for it
    inode = searchForFile(dirname, read_block, temp_fil);
    readBlock(mountedDiskNum, inode, read_block);
    //check if it's a directory
    if (read_block[0] != '5' || read_block[1] != 0x44)
    {
        return -1;
    }
    //set the buffer to be a free block ready to overwrite the deleted directory block
    buffer[0] = '4';
    buffer[1] = 0x44;
    buffer[2] = read_block[2];
    //Bytes 4-12 is reserved for directory name
    for (i = 13; i < BLOCKSIZE; i++)
    {
        //error if we somehow reach a non-null character
        if (read_block[i] != 0x00)
        {
            return -1;
        }
    }
    //HAVE TO DELETE DIRECTORY FROM ROOT DIRECTORY SOMEHOW SOMEWAY
    //if pass through, directory is empty 
    memset(buffer, 0x00, BLOCKSIZE);
    writeBlock(mountedDiskNum, inode, buffer);

    free(read_block);
    free(buffer);
    free(temp_fil);
    return 1;
}

int tfs_seek(fileDescriptor FD, int offset){

    char* temp_fil = (char*)malloc(sizeof(char) * 9);
    char* read_block = (char*)malloc(sizeof(char) * BLOCKSIZE);

    //read from superblock, get curr directory file
    readBlock(mountedDiskNum,0,read_block);
    int root_inode = read_block[5];
    readBlock(mountedDiskNum,root_inode,read_block);
    int cur_directory = read_block[2];
    readBlock(mountedDiskNum,cur_directory,read_block);
    //find filename from filedescriptor and find correct inode file
    Node* node = findNode(FD);
    if (node == NULL){
        free(temp_fil);
        free(read_block);
        return -1;    
    }
    char* filename = node -> fileName;
    int inode = searchForFile(filename, read_block, temp_fil);
   
    readBlock(mountedDiskNum, inode, read_block); 
    //set file_pointer to offset
    int blocksToRead = (offset - (offset % (BLOCKSIZE-4))) / (BLOCKSIZE-4);
    int currByte = offset % (BLOCKSIZE - 4);
    
    read_block[14] = currByte;
    read_block[15] = blocksToRead;
    return writeBlock(mountedDiskNum, inode, read_block);
}

int tfs_readByte(fileDescriptor FD, char* buffer){
    char* read_block = (char*)malloc(sizeof(char) * BLOCKSIZE);
    char* temp_fil = (char*)malloc(sizeof(char) * 9);
    int blocksToRead, currByte;

    //read from superblock, get curr directory file
    readBlock(mountedDiskNum,0,read_block);
    int root_inode = read_block[5];
    readBlock(mountedDiskNum,root_inode,read_block);
    int cur_directory = read_block[2];
    readBlock(mountedDiskNum,cur_directory,read_block);

    //find filename from filedescriptor and find correct inode file
    Node* fil = (findNode(FD));
    if (fil == NULL)
    {
        free(read_block);
        free(temp_fil); 
        return -1;
    }
    char* filename = fil->fileName;
    int inode = searchForFile(filename, read_block, temp_fil);
    readBlock(mountedDiskNum,inode,read_block);

    //get size of file, file_pointer, and first file_extent from inode 
    int file_size;
    if (read_block[12] == 0){
        file_size = (read_block[13]) * (BLOCKSIZE-4);
    }else{
        file_size = read_block[12] + ((read_block[13]-1) * (BLOCKSIZE-4));
    }
     
    char fp_bytes = read_block[14];
    char fp_blocks = read_block[15];
    int file_pointer = fp_bytes + fp_blocks*(BLOCKSIZE-4); 
    int file_extent = read_block[2];

    if (file_pointer >= file_size)
    {
        //error if file pointer past the end of the file
        return -1;
    }
    else
    {
        //change file_pointer to += 1 and write it back to disk
        tfs_seek(FD, file_pointer + 1);

        //if 0-251, 0 blocks, if 252-503, 1 blocks, etc...
        blocksToRead = (file_pointer - (file_pointer % (BLOCKSIZE-4))) / (BLOCKSIZE-4);
        currByte = file_pointer % (BLOCKSIZE - 4);
        

        // reads the first file extent and continues if necessary 
        while (blocksToRead >= 0)
        {
            //now read_block contains next file_extent 
            readBlock(mountedDiskNum, file_extent, read_block);
            file_extent = read_block[2]; //next file_extent file if needed
            blocksToRead = blocksToRead - 1;
        }

        //copies the current byte pointed by filepointer into buffer?
        buffer[0] = read_block[currByte+4];
        buffer[1] = '\0';
        // strncpy(buffer, read_block[currByte], 1);
        //valid af
        return 1;

    }
}

/*
int main(){
    printf("%d\n",tfs_mkfs("disk0.dsk",2562));
    tfs_mount("disk0.dsk");
    fileDescriptor fd = tfs_openFile("/ritvik");
    printf("fd %d\n",fd);
    fileDescriptor fd2 = tfs_openFile("/joel");
    printf("fd %d\n",fd);
    fileDescriptor fd3 = tfs_openFile("/joel");
    char* message = "hi my name is Joel. Tymon is an asshole! What does life mean? Computer Science is the worst major in the world!AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAABBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB";
    printf("strlen %d\n", strlen(message));
    int err = tfs_writeFile(fd,message,strlen(message));
    printf("writeFile %d\n",err);
    tfs_readdir(); 
    
    char* buffer = (char *)malloc(sizeof(char) * 2);
    tfs_readByte(fd,buffer);
    printf("buffer: %s\n",buffer); 
    tfs_readByte(fd,buffer);
    printf("buffer: %s\n",buffer); 
    tfs_seek(fd,509); 
    tfs_readByte(fd,buffer);
    printf("buffer: %s\n",buffer); 
    err = tfs_readByte(fd,buffer);
    printf("%d\n",err);
    tfs_deleteFile(fd);

    tfs_createDir("/temp");

    fileDescriptor fd4 = tfs_openFile("/temp/test");
    //printf("%d\n",fd4);
    tfs_createDir("/temp/subdir");
    err = tfs_rename(fd2,"tymon");
    printf("err %d\n", err);
    tfs_unmount();
    return 0;
}
*/
