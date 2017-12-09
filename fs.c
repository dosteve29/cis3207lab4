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
short nextFreeEntry(directory *dir);
void createFile(FAT * fat, DATA * data, int dirBlock, char * filename, char * ext);
void createDir(FAT * fat, DATA * data, int dirBlock, char * filename);
void writeToFile(FAT * fat, DATA * data, int dirBlock, char * filename, char * ext, char * buf);
int findFileEntry(directory * dir, char * filename, char * ext);

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
    createFile(fat, data, 0, "steve", "txt");
    createDir(fat, data, 0, "giraffe");
    writeToFile(fat, data, 0, "steve", "txt", "Hello this is steve");

    /* //3. copy the data to appropriate DATA block */
    /* char * stringToWrite = "Hello this is steve"; */
    /* strcpy(data->blocks[freeBlock].sect, stringToWrite); */
    
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

short nextFreeEntry(directory *dir){
    short i;
    for (i = 0; i < MAX_ENTRIES; i++){
        if (dir->entry[i].startingIndex == 0){
            return i;
        }
    }
    return -1;
}

void createFile(FAT * fat, DATA * data, int dirBlock, char * filename, char * ext){
    //open the current directory
    directory *dir = malloc(sizeof(*dir));
    memcpy(dir, &(data->blocks[dirBlock]), sizeof(*dir));

    //find next free block and fill up FAT
    short freeBlock = nextFreeBlock(fat);
    fat->file[freeBlock].busy = 1;
    fat->file[freeBlock].next = -1;

    //find next free entry in the dirBlock
    short freeEntry = nextFreeEntry(dir);

    //edit the directory entry metadata for the file
    editFileSize(&dir->entry[freeEntry].fileSize, 0);
    editIndex(&dir->entry[freeEntry].startingIndex, freeBlock);
    editFolder(&dir->entry[freeEntry].folder, 0);
    getTime(dir->entry[freeEntry].time);
    getDate(dir->entry[freeEntry].date);
    editExt(dir->entry[freeEntry].extension, ext);
    editFilename(dir->entry[freeEntry].filename, filename);
    memcpy(&(data->blocks[dirBlock]), dir, sizeof(*dir));
}

void createDir(FAT * fat, DATA * data, int dirBlock, char * filename){
    //open the current directory
    directory *dir = malloc(sizeof(*dir));
    memcpy(dir, &(data->blocks[dirBlock]), sizeof(*dir));

    //find next free block and fill up FAT
    short freeBlock = nextFreeBlock(fat);
    fat->file[freeBlock].busy = 1;
    fat->file[freeBlock].next = -1;

    short freeEntry = nextFreeEntry(dir);

    //edit the directory entry metadata for the file
    editFileSize(&dir->entry[freeEntry].fileSize, 0);
    editIndex(&dir->entry[freeEntry].startingIndex, freeBlock);
    editFolder(&dir->entry[freeEntry].folder, 1);
    getTime(dir->entry[freeEntry].time);
    getDate(dir->entry[freeEntry].date);
    editExt(dir->entry[freeEntry].extension, "   ");
    editFilename(dir->entry[freeEntry].filename, filename);
    memcpy(&(data->blocks[dirBlock]), dir, sizeof(*dir));
}

void writeToFile(FAT * fat, DATA * data, int dirBlock, char * filename, char * ext, char * buf){
    //open the current directory
    directory *dir = malloc(sizeof(*dir));
    memcpy(dir, &(data->blocks[dirBlock]), sizeof(*dir));

    //get the entry with the filename and extension
    int file = findFileEntry(dir, filename, ext);
    if (file == -1){
        printf("File not found!\n");
        return;
    } else{
        int dataBlock = dir->entry[file].startingIndex;
        printf("File located in block: %d\n", dataBlock);
        strcpy(data->blocks[dataBlock].sect, buf);
        editFileSize(&dir->entry[file].fileSize, strlen(buf));
    }
    memcpy(&(data->blocks[dirBlock]), dir, sizeof(*dir));
}

int findFileEntry(directory * dir, char * filename, char * ext){
    int i;
    for (i = 0; i < MAX_ENTRIES; i++){
        if (((strcmp(dir->entry[i].filename, filename)) == 0) && ((strcmp(dir->entry[i].extension, ext)) == 0)){
            return i;
        }
    }
    return -1;
}
