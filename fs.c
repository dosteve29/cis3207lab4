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
void createFile(FAT * fat, DATA * data, char * filename, char * ext);
void createDir(FAT * fat, DATA * data, char * filename);
void writeToFile(FAT * fat, DATA * data, char * filename, char * ext, char * buf);
int findFileOffset(FAT * fat, DATA * data, directory * dir, int file);
int findFileEntry(directory * dir, char * filename, char * ext);
void deleteFile(FAT * fat, DATA * data, char * filename, char * ext);
void readFile(FAT * fat, DATA * data, char * filename, char * ext);
int isEmpty();
int isFull();
int peek();
int pop();
void push(int dirBlock);
int cd(DATA * data, char * dirName);

//current directory stack
int stack[BLOCKS];
int top = 0; 

//MAIN STARTS HERE
int main(int argc, char ** args){
    //pointers to each partition of disk
    FAT *fat;
    DATA *data;
    Entry *root;

    //the first element of directory stack will always be 0, the root dir
    stack[top] = 0; 

    //create file large enough to hold both FAT, DATA, and root
    int fd = open("Drive", O_RDWR|O_CREAT, 0660);
    ftruncate(fd, sizeof(FAT) + sizeof(Entry) + BLOCKS * BLOCK_SIZE);
    fat = mmap(NULL, sizeof(FAT), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    data = mmap(NULL, sizeof(DATA), PROT_READ | PROT_WRITE, MAP_SHARED, fd, sizeof(FAT));
    root = mmap(NULL, sizeof(Entry), PROT_READ | PROT_WRITE, MAP_SHARED, fd, sizeof(FAT) + sizeof(DATA));

    //initialize the file system 
    fs_initialize(fat, root);
    createDir(fat, data, "penguin");
    if (cd(data, "penguin")){
        createFile(fat, data, "steve", "txt");
        char buff[1500];
        memset(buff, '\0', sizeof(buff));
        int i;
        for (i = 0; i < 1499; i++){
            buff[i] = 'h';
        }
        writeToFile(fat, data, "steve", "txt", buff);
        writeToFile(fat, data, "steve", "txt", "Hello this is steve");
        readFile(fat, data, "steve", "txt");
        /* deleteFile(fat, data, "steve", "txt"); */
        /* createFile(fat, data, "steve", "txt"); */
        /* writeToFile(fat, data, "steve", "txt", "deleted file"); */
        /* writeToFile(fat, data, "steve", "txt", "deleted file"); */
        /* writeToFile(fat, data, "steve", "txt", "deleted file"); */
    } else{
        printf("couldn't find the directory!\n");
    }
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

void createFile(FAT * fat, DATA * data, char * filename, char * ext){
    //open the current directory
    directory *dir = malloc(sizeof(*dir));
    memcpy(dir, &(data->blocks[peek()]), sizeof(*dir));

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

    //rewrite the data block with edited info
    memcpy(&(data->blocks[peek()]), dir, sizeof(*dir));
}

void createDir(FAT * fat, DATA * data, char * filename){
    //open the current directory
    directory *dir = malloc(sizeof(*dir));
    memcpy(dir, &(data->blocks[peek()]), sizeof(*dir));

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
    memcpy(&(data->blocks[peek()]), dir, sizeof(*dir));
}

void writeToFile(FAT * fat, DATA * data, char * filename, char * ext, char * buf){
    //open the current directory
    directory *dir = malloc(sizeof(*dir));
    memcpy(dir, &(data->blocks[peek()]), sizeof(*dir));

    //get the entry with the filename and extension
    int file = findFileEntry(dir, filename, ext);
    if (file == -1){
        printf("File not found!\n");
        return;
    } else{
        int startingBlock = dir->entry[file].startingIndex;
        while (fat->file[startingBlock].next != -1){
            startingBlock = fat->file[startingBlock].next;
        }
        int bufLength = strlen(buf);
        while (bufLength > 0){
            printf("Starting write at data block: %d\n", startingBlock);
            printf("BufLength: %d\n", bufLength);
            int offset = findFileOffset(fat, data, dir, file);
            printf("Offset: %d\n", offset);
            int n = (bufLength < (BLOCK_SIZE - offset)) ? bufLength : BLOCK_SIZE - offset;
            printf("N: %d\n", n);
            strncpy(data->blocks[startingBlock].sect + offset, buf + (strlen(buf) - bufLength), n);
            editFileSize(&dir->entry[file].fileSize, n + dir->entry[file].fileSize); 
            printf("Filesize now is: %d\n", dir->entry[file].fileSize);
            bufLength = bufLength - n;
            if ((offset + n) == BLOCK_SIZE){
                int freeBlock = nextFreeBlock(fat);
                fat->file[startingBlock].next = freeBlock;
                fat->file[freeBlock].busy = 1;
                fat->file[freeBlock].next = -1;
                startingBlock = freeBlock;
            }
        }
    }
    getTime(dir->entry[file].time);
    getDate(dir->entry[file].date);
    memcpy(&(data->blocks[peek()]), dir, sizeof(*dir));
}

int findFileOffset(FAT * fat, DATA * data, directory * dir, int file){
    int filesize = dir->entry[file].fileSize;
    int start = dir->entry[file].startingIndex;
    int count = 0;
    while (fat->file[start].next != -1){
        start = fat->file[start].next;
        count++; 
    }
    return filesize - (BLOCK_SIZE * count);
}

int findFileEntry(directory * dir, char * filename, char * ext){
    int i;
    for (i = 0; i < MAX_ENTRIES; i++){
        if (((strcmp(dir->entry[i].filename, filename)) == 0) && //looks for same filename
                ((strcmp(dir->entry[i].extension, ext)) == 0) && //looks for same extension
                (dir->entry[i].startingIndex != 0)){ //looks if the entry is a deleted file
            return i;
        }
    }
    return -1;
}

void deleteFile(FAT * fat, DATA * data, char * filename, char * ext){
    directory *dir = malloc(sizeof(*dir));
    memcpy(dir, &(data->blocks[peek()]), sizeof(*dir));

    int i = findFileEntry(dir, filename, ext);
    if (i == -1){
        printf("Can't find file!\n");
        return;
    } else{
        int block = dir->entry[i].startingIndex;
        while (fat->file[block].next != -1){
            int temp = fat->file[block].next;
            fat->file[block].busy = 0;
            fat->file[block].next = -1;
            block = temp;
        }
        fat->file[block].busy = 0;
        fat->file[block].next = -1;
        editFileSize(&dir->entry[i].fileSize, 0);
        editIndex(&dir->entry[i].startingIndex, 0);
    } 

    memcpy(&(data->blocks[peek()]), dir, sizeof(*dir));
}

void readFile(FAT * fat, DATA * data, char * filename, char * ext){
    directory *dir = malloc(sizeof(*dir));
    memcpy(dir, &(data->blocks[peek()]), sizeof(*dir));
    int file = findFileEntry(dir, filename, ext);
    if (file == -1){
        printf("Can't find file!\n");
    } else{
        char buf[dir->entry[file].fileSize + 1];
        memset(buf, '\0', sizeof(buf));
        int startBlock = dir->entry[file].startingIndex;
        char temp[BLOCK_SIZE + 1];

        while(fat->file[startBlock].next != -1){
            memset(temp, '\0', sizeof(temp));
            memcpy(temp, data->blocks[startBlock].sect, 512);
            strcat(buf, temp);
            startBlock = fat->file[startBlock].next;
        }
        int offset = findFileOffset(fat, data, dir, file);
        memset(temp, '\0', sizeof(temp));
        memcpy(temp, data->blocks[startBlock].sect, offset);
        strcat(buf, temp);
        printf("%s\n", buf);
    }
    memcpy(&(data->blocks[peek()]), dir, sizeof(*dir));
}

int isEmpty(){
    if (top == 0)
        return 1;
    else
        return 0;
}

int isFull(){
    if (top == BLOCKS)
        return 1;
    else
        return 0;
}

int peek(){
   return stack[top]; 
}

int pop(){
    int data;

    if (!isEmpty()){
        data = stack[top];
        top = top - 1;
        return data;
    } else{
        return 0;
    }
}

void push(int dirBlock){
    if (!isFull()){
        top = top + 1;
        stack[top] = dirBlock;
    } else{
        printf("Stack Full\n");
    } 
}

int cd(DATA * data, char * dirName){
    //go to previous directory
    if (strcmp(dirName, "..") == 0){
        pop();
        return 1;
    }

    directory * dir = malloc(sizeof(*dir));
    memcpy(dir, &(data->blocks[peek()]), sizeof(*dir));

    int i;
    for (i = 0; i < MAX_ENTRIES; i++){
        if ((strcmp(dirName, dir->entry[i].filename) == 0) && (dir->entry[i].folder == 1)){
            push(dir->entry[i].startingIndex);
            return 1;
        } 
    }
    return 0;
}


