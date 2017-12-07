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
#define BLOCK_SIZE 1024

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
//each entry is 64 bytes long
typedef struct entry{
    unsigned short fileSize; //2 bytes
    char time[9]; //9 bytes
    char date[9]; //9 bytes;
    unsigned short startingIndex; //2 bytes
    char folder; //1 byte
    char extension[4]; //4 bytes (3 extension + terminator)
    char filename[37]; //37 bytes (36 filename + terminator)
} Entry;

//Directory Entry Table that occupies each 1024 byte
//since each entry is 64 bytes long,
//each data block or directory table can hold 16 entries
//64 * 16 = 1024;
typedef struct directoryTable{
    Entry entry[16];
} directory;

typedef struct sector{
    char sect[BLOCK_SIZE];
} Sector;

typedef struct dataBlock{
    Sector blocks[BLOCKS];
} DATA;

//function prototype
void getDate(char *dateString);
void getTime(char *timeString);
void fs_initialize(int fd, FAT *fat, DATA *data, Entry *root);
void editFileSize(unsigned short oldSize, unsigned short newSize);
void editIndex(unsigned short oldIndex, unsigned short newIndex);
void editFolder(char oldFolder, char newFolder);
void editExt(char * oldExtension, char * newExtension);
void editFilename(char * oldFilename, char * newFilename);

int main(int argc, char ** args){
    FAT *fat;
    DATA *data;
    Entry *root;

    //create file large enough to hold both FAT and 512 byte blocks
    int fd = open("Drive", O_RDWR|O_CREAT, 0660);
    ftruncate(fd, sizeof(FAT) + sizeof(Entry) + BLOCKS * BLOCK_SIZE);
    fat = mmap(NULL, sizeof(FAT), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    data = mmap(NULL, sizeof(DATA), PROT_READ | PROT_WRITE, MAP_SHARED, fd, sizeof(FAT));
    root = mmap(NULL, sizeof(Entry), PROT_READ | PROT_WRITE, MAP_SHARED, fd, sizeof(FAT) + sizeof(DATA));

    fs_initialize(fd, fat, data, root);

    close(fd);
    exit(0);
}

void fs_initialize(int fd, FAT *fat, DATA *data, Entry *root){
   //initialize the root directory metadata
   editFileSize(root->fileSize, 123);
   editIndex(root->startingIndex, 0);
   editFolder(root->folder, 1);
   getTime(root->time);
   getDate(root->date);
   editExt(root->extension, "   ");
   editFilename(root->filename, "root");
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

void editFileSize(unsigned short oldSize, unsigned short newSize){
    oldSize = newSize;
}
void editIndex(unsigned short oldIndex, unsigned short newIndex){
    oldIndex = newIndex;
}
void editFolder(char oldFolder, char newFolder){
    oldFolder = newFolder;
}
void editExt(char * oldExtension, char * newExtension){
    strcpy(oldExtension, newExtension);
}
void editFilename(char * oldFilename, char * newFilename){
    strcpy(oldFilename, newFilename);
}
