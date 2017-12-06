#include <string.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <stdlib.h>

#define BLOCKS 4096

//this is each individual entry in the file allocation table
typedef struct fileAllocationEntry{
    short busy;
    short next; 
} fileEntry;

//FAT struct
typedef struct fileAllocationTable{
    fileEntry file[BLOCKS]; //there are 4096 entries or data blocks recorded by FAT
} FAT;

//Each entry in directory entry table structure
//each entry is 32 bytes long
typedef struct entry{
    unsigned short fileSize; //2 bytes
    unsigned int lastModified; //8 bytes
    unsigned short startingIndex; //2 bytes
    char folder; //1 byte
    char extension[3]; //3 bytes
    char filename[16]; //16 bytes
} Entry;

//Directory Entry Table that occupies each 512 byte
//since each entry is 32 bytes long,
//each data block or directory table can hold 16 entries
//32 * 16 = 512
typedef struct directoryTable{
    Entry entry[16];
} directory;

typedef struct sector{
    char sect[512];
} Sector;

typedef struct dataBlock{
    Sector blocks[BLOCKS];
} DATA;

void editFAT(FAT *fat, int index);

int main(int argc, char ** args){
    //create file large enough to hold both FAT and 512 byte blocks
    int fd = open("Drive", O_RDWR|O_CREAT, 0660);
    ftruncate(fd, sizeof(FAT) + 32 + BLOCKS * 512);

    //map each section of drive to a mmap pointer
    FAT *fat = mmap(0, sizeof(FAT), PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
    DATA *data = mmap(0, sizeof(DATA), PROT_READ|PROT_WRITE, MAP_SHARED, fd, sizeof(FAT));
    Entry *root = mmap(0, 32, PROT_READ|PROT_WRITE, MAP_SHARED, fd, sizeof(FAT) + (BLOCKS * 512));

    /* char * string = "Hello"; */
    /* memcpy(&data->blocks[0], string, sizeof(directory)); */

    /* directory rootDir; */
    /* rootDir.entry[0].fileSize = 30000; */
    /* rootDir.entry[0].lastModified = 152; */
    /* rootDir.entry[0].startingIndex = 12; */
    /* rootDir.entry[0].folder = 0; */
    /* char * ext = "txt"; */
    /* char * name = "giraffe"; */
    /* strcpy(rootDir.entry[0].extension, ext); */
    /* strcpy(rootDir.entry[0].filename, name); */
    /*  */
    /* memcpy(&data->blocks[0], &rootDir, sizeof(Sector)); */
    /*  */
    /* printf("%s\n", rootDir2.name);  */

    close(fd);
    exit(0);
}

void editFAT(FAT *fat, int index){
    fat->file[index].busy = 1;
    fat->file[index].next = -1;
}
