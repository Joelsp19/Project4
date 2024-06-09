#define BLOCKSIZE 256
#define DEFAULT_DISK_SIZE 10240
#define DEFAULT_DISK_NAME "tinyFSDisk"
typedef int fileDescriptor;

extern int tfs_mkfs(char* filename, int nBytes);
extern int tfs_mount(char* diskname);
extern int tfs_closeFile(fileDescriptor FD);
extern int tfs_unmount(void);
extern int addNewFile(char* name);
extern fileDescriptor tfs_openFile(char* name);
extern int tfs_writeFile(fileDescriptor FD, char* buffer, int size);
extern int tfs_deleteFile(fileDescriptor FD);
extern int tfs_readByte(fileDescriptor FD, char* buffer);
extern int tfs_seek(fileDescriptor FD, int offset);
extern int tfs_readdir();
extern int tfs_removeAll(char *dirname);
extern int tfs_removeDir(char *dirname);
extern int tfs_createDir(char *dirname);
