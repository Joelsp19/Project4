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

int mountedDiskNum = -1;
char* mountedDiskName = NULL;

int tfs_mkfs(char* filename, int nBytes){
    char write_block[BLOCKSIZE];
    int diskNum = openDisk(filename,nBytes);
    int err_code;
    if (diskNum < 0){
        return diskNum;
    }
    // format the file
    // set the file to 0
    memset(write_block,0x00,sizeof(write_block));
    write_block[0] = 4;
    write_block[1] = MAGIC_NUMBER;
    int i;
    int numBlocks = ((nBytes - (nBytes % BLOCKSIZE)) / BLOCKSIZE);
    if (numBlocks < 2){
        return -1;
    }

    for (i=0; i<numBlocks; i++){
        if (i == numBlocks-1){
            write_block[2] = 0x0; //empty next free block (no more free blocks)
        }else{
            write_block[2] = i+1; //defaults the next free block to be the next available block
        }
        err_code=writeBlock(diskNum,i,write_block); 
        if (err_code < 0){
            return err_code; 
        }
    }
    // set the superblock
    char super_block[BLOCKSIZE];
    memset(super_block,0x00,sizeof(super_block));
    super_block[0] = 1;
    super_block[1] = MAGIC_NUMBER;
    super_block[2] = 2;//pointer to next free block
    super_block[3] = 0x00;  //empty 
    super_block[4] = MAGIC_NUMBER;//magic number
    super_block[5] = 1;//pointer to root inode
    super_block[6] = numBlocks; //size of the disk
    
    //set the inodeblock
    char root_inode[BLOCKSIZE];
    memset(root_inode,0x00,sizeof(root_inode));
    root_inode[0] = 2;
    root_inode[1] = MAGIC_NUMBER;
    root_inode[2] = 0x00;
    root_inode[3] = 0x00;  //empty      
 
    err_code = writeBlock(diskNum,0,super_block);
    if (err_code < 0){
        return err_code;
    }
    err_code = writeBlock(diskNum,1,root_inode);
    if (err_code < 0){
        return err_code;
    }
    // the disk is set up
    // lets close it for now
    err_code = closeDisk(diskNum);
    if (err_code < 0){
        return err_code;
    } 
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

    int diskNum; 
    diskNum = openDisk(diskname, 0); 
    if (diskNum < 0){
        return diskNum;
    }
    //lets read the superblock
    char read_block[BLOCKSIZE];
    int err_code;
    err_code = readBlock(diskNum,0,read_block);
    if (err_code < 0){
        return err_code;
    }
    if (read_block[1] != MAGIC_NUMBER){
        return ERR_INVALID_TINYFS; 
    }
    int numBlocks = read_block[6];
    int i;
    for (i=0;i<numBlocks;i++){
        if (read_block[1] != MAGIC_NUMBER){
            return ERR_INVALID_TINYFS; 
        }
    }
    mountedDiskNum = diskNum;
    mountedDiskName = diskname;
    return SUCCESS;

}

int tfs_unmount(void){
    // lets unmount the currently mounted file
    //close all the open files

    mountedDiskNum = -1;
    mountedDiskName = NULL;
    return SUCCESS;     


}

fileDescriptor tfs_openFile(char* name){

}

int tfs_closeFile(fileDescriptor FD){


}

int tfs_writeFile(fileDescriptor FD, char* buffer, int size){

}

int tfs_deleteFile(fileDescriptor FD){

}

int tfs_readByte(fileDescriptor FD, char* buffer){

}

int tfs_seek(fileDescriptor FD, int offset){

}

int main(){
    tfs_mkfs("disk0.dsk",2562);
    tfs_mount("disk0.dsk");
    printf("%d\n",tfs_mount("disk1.dsk"));
    tfs_unmount();
    return 0;
}
