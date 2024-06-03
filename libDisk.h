extern int openDisk(char* filename, int nBytes);
extern int closeDisk(int disk);
extern int readBlock(int disk, int bNum, void *block);
extern int writeBlock(int disk, int bNumm, void *block);


