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
#define BLOCK_SIZE 512
#define MAX_ENTRIES 8

//this is each individual entry in the file allocation table
typedef struct fileAllocationEntry{
    short busy;
    short next; 
} fileEntry;

//FAT struct
typedef struct fileAllocationTable{
    fileEntry file[BLOCKS]; //there are BLOCKS number of fileEntry in the FAT
} FAT;

//Each entry in directory entry table structure
//each entry is 64 bytes long
typedef struct entry{
    unsigned short fileSize; //2 bytes
    unsigned short startingIndex; //2 bytes
    char folder; //1 byte
    char time[9]; //9 bytes (8 time + terminator)
    char date[9]; //9 bytes; (8 time + terminator)
    char filename[37]; //37 bytes (36 filename + terminator)
    char extension[4]; //4 bytes (3 extension + terminator)
} Entry;

//Directory Entry Table that occupies a 512 byte data block
//since each entry is 64 bytes long,
//each data block or directory table can hold 8 entries
//64 * 8 = 512;
typedef struct directoryTable{
    Entry entry[MAX_ENTRIES];
} directory;

//this is individual block
typedef struct sector{
    char sect[BLOCK_SIZE];
} Sector;

//this is collection of all data blocks
typedef struct dataBlock{
    Sector blocks[BLOCKS];
} DATA;

//function prototype
void getDate(char *dateString);
void getTime(char *timeString);
void fs_initialize(FAT *fat, Entry *root);
void editFileSize(unsigned short * oldSize, unsigned short newSize);
void editIndex(unsigned short *oldIndex, unsigned short newIndex);
void editFolder(char *oldFolder, char newFolder);
void editExt(char * oldExtension, char * newExtension);
void editFilename(char * oldFilename, char * newFilename);
short nextFreeBlock(FAT * fat);

//MAIN STARTS HERE
int main(int argc, char ** args){
    //pointers to each partition of disk
    FAT *fat;
    DATA *data;
    Entry *root;

    //create file large enough to hold both FAT, DATA, and root
    int fd = open("Drive", O_RDWR|O_CREAT, 0660);
    ftruncate(fd, sizeof(FAT) + sizeof(Entry) + BLOCKS * BLOCK_SIZE);
    fat = mmap(NULL, sizeof(FAT), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    data = mmap(NULL, sizeof(DATA), PROT_READ | PROT_WRITE, MAP_SHARED, fd, sizeof(FAT));
    root = mmap(NULL, sizeof(Entry), PROT_READ | PROT_WRITE, MAP_SHARED, fd, sizeof(FAT) + sizeof(DATA));

    //initialize the file system 
    fs_initialize(fat, root);

    //create a file in root directory
    directory *rootDir = malloc(sizeof(*rootDir));
    memcpy(rootDir, &(data->blocks[0]), sizeof(*rootDir));

    /* rootDir->entry[0].startingIndex = 3; */
    /* int i; */
    /* for (i = 0; i < MAX_ENTRIES; i++){ */
    /*     printf("Entry %d    Starting index: %d\n", i, rootDir->entry[i].startingIndex); */
    /* } */

    //1. find the next free block in FAT
    short freeBlock = nextFreeBlock(fat);
    //2. set the free block as busy now
    fat->file[freeBlock].busy = 1;
    fat->file[freeBlock].next = -1;
    //3. copy the data to appropriate DATA block
    char * stringToWrite = "Hello this is steve";
    strcpy(data->blocks[freeBlock].sect, stringToWrite);
    //4. edit directory metadata
    editFileSize(&rootDir->entry[0].fileSize, strlen(stringToWrite));
    editIndex(&rootDir->entry[0].startingIndex, freeBlock);
    editFolder(&rootDir->entry[0].folder, 0);
    getTime(rootDir->entry[0].time);
    getDate(rootDir->entry[0].date);
    editExt(rootDir->entry[0].extension, "txt");
    editFilename(rootDir->entry[0].filename, "steve");
    memcpy(&(data->blocks[0]), rootDir, sizeof(*rootDir));

    close(fd);
    exit(0);
}

void fs_initialize(FAT *fat, Entry *root){
   //initialize the root directory metadata
   editFileSize(&(root->fileSize), 0);
   editIndex(&(root->startingIndex), 0);
   editFolder(&(root->folder), 1);
   getTime(root->time);
   getDate(root->date);
   editExt(root->extension, "   ");
   editFilename(root->filename, "root");

   //initialize the FAT
   fat->file[0].busy = 1;
   fat->file[0].next = -1;

}

void getTime(char *timeString){
    time_t current_time;
    struct tm * time_info;

    time(&current_time);
    time_info = localtime(&current_time);
    char time[9];
    strftime(time, sizeof(time), "%X", time_info);
    strcpy(timeString, time);
}

void getDate(char *dateString){
    time_t current_time;
    struct tm * time_info;
    time(&current_time);
    time_info = localtime(&current_time);
    char date[9];

    strftime(date, sizeof(date), "%x", time_info);
    strcpy(dateString, date);
}

void editFileSize(unsigned short *oldSize, unsigned short newSize){
    *oldSize = newSize;
}
void editIndex(unsigned short *oldIndex, unsigned short newIndex){
    *oldIndex = newIndex;
}
void editFolder(char *oldFolder, char newFolder){
    *oldFolder = newFolder;
}
void editExt(char * oldExtension, char * newExtension){
    strcpy(oldExtension, newExtension);
}
void editFilename(char * oldFilename, char * newFilename){
    strcpy(oldFilename, newFilename);
}

short nextFreeBlock(FAT * fat){
    short i;
    for (i = 1; i < BLOCKS; i++){
        if (fat->file[i].busy == 0){
            return i;
        }
    }
    return -1;
}
