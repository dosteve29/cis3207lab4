//Hyunseung Do
//CIS3207 Lab 4

//header files
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <stdlib.h>

//defined constants
#define BLOCKS 4096 //number of blocks
#define BLOCK_SIZE 512 //size of each block
#define MAX_ENTRIES 8 //number of entries per block

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

//function prototypes
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

    //testing application
    createDir(fat, data, "penguin"); //1. create a directory below the root of your file system
    if (cd(data, "penguin")){ //change directory to created directory
        createFile(fat, data, "steve", "txt"); //2. create a file in that directory
        /* //3. write a specified formatted data set into the file */
        char buff[1500]; //writing 1500 (1499 char + null terminator) to demonstrate linking data blocks for a file
        memset(buff, '\0', sizeof(buff)); 
        int i;
        for (i = 0; i < 1499; i++){
            buff[i] = 'h';
        }
        writeToFile(fat, data, "steve", "txt", buff); //writing the buffer to the file
        writeToFile(fat, data, "steve", "txt", "Hello this is steve"); //writing random stuff to the file at the end
        readFile(fat, data, "steve", "txt"); //4. read back and compare the data that have been written to the file

        deleteFile(fat, data, "steve", "txt");
        createFile(fat, data, "giraffe", "txt");
        writeToFile(fat, data, "giraffe", "txt", "deleted file");
        readFile(fat, data, "giraffe", "txt");
    } else{ //if cd fails
        printf("couldn't find the directory!\n");
    }
    close(fd); //close the file descriptor
    exit(0);
}

//this initializes the root directory
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

//get last modification time
void getTime(char *timeString){
    time_t current_time;
    struct tm * time_info;

    time(&current_time);
    time_info = localtime(&current_time);
    char time[9];
    strftime(time, sizeof(time), "%X", time_info);
    strcpy(timeString, time);
}

//get last modification date
void getDate(char *dateString){
    time_t current_time;
    struct tm * time_info;
    time(&current_time);
    time_info = localtime(&current_time);
    char date[9];

    strftime(date, sizeof(date), "%x", time_info);
    strcpy(dateString, date);
}

/*
 * replace old information with new information
 * on various metadata
 */
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

//find the next free block in the FAT
short nextFreeBlock(FAT * fat){
    short i;
    for (i = 1; i < BLOCKS; i++){
        if (fat->file[i].busy == 0){ //busy is either 0 or 1. 
            return i;
        }
    }
    return -1;
}

//find the next free entry in the directory block
short nextFreeEntry(directory *dir){
    short i;
    for (i = 0; i < MAX_ENTRIES; i++){
        //if the starting index is 0, then it is unused entry
        //since root directory always occupies data block 0,
        //0 is used for indication of free entry
        if (dir->entry[i].startingIndex == 0){
            return i;
        }
    }
    return -1;
}

//creating the file
//FAT is required to search for free block and update the block's busy and next state
//DATA is required to provide the current directory entries
//filename and ext are required for file metadata
void createFile(FAT * fat, DATA * data, char * filename, char * ext){
    //open the current directory
    directory *dir = malloc(sizeof(*dir));
    memcpy(dir, &(data->blocks[peek()]), sizeof(*dir)); //peek() is used to get the current directory block

    //find next free block and fill up FAT
    short freeBlock = nextFreeBlock(fat);
    fat->file[freeBlock].busy = 1;
    fat->file[freeBlock].next = -1;

    //find next free entry in the current directory
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

//creating the directory
//since file and subdirectories are treated the same,
//the only difference is the folder metadata, which is 1 here.
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

//writing data to file
//FAT is required to search for next data blocks and such
//DATA is required to provide the actual storage for filesystem
//filename, ext are used to locate the file
//buf is the data to write to the file
void writeToFile(FAT * fat, DATA * data, char * filename, char * ext, char * buf){
    //open the current directory
    directory *dir = malloc(sizeof(*dir));
    memcpy(dir, &(data->blocks[peek()]), sizeof(*dir));

    //get the entry with the filename and extension
    int file = findFileEntry(dir, filename, ext);

    if (file == -1){ //open file failure
        printf("File not found!\n");
        return;
    } else{ //open file success
        int startingBlock = dir->entry[file].startingIndex; //search the directory metadata for starting index
        while (fat->file[startingBlock].next != -1){ //since data is appended to the end of the data block chain, search for the last data block
            startingBlock = fat->file[startingBlock].next;
        }
        int bufLength = strlen(buf); //this is the length of the buffer
        while (bufLength > 0){ //while there is more buffer to write
            printf("Starting write at data block: %d\n", startingBlock); 
            printf("BufLength: %d\n", bufLength);
            int offset = findFileOffset(fat, data, dir, file); //offset is the number of bytes that are already used in the data block
            printf("Offset: %d\n", offset); 
            int n = (bufLength < (BLOCK_SIZE - offset)) ? bufLength : BLOCK_SIZE - offset; //determine whether the remaining buffer to write is more than the BLOCK_SIZE or not 
            printf("N: %d\n", n);
            strncpy(data->blocks[startingBlock].sect + offset, buf + (strlen(buf) - bufLength), n); //from the offset, write appropriate amount of data to fill up the data block size. 
            editFileSize(&dir->entry[file].fileSize, n + dir->entry[file].fileSize); //edit the metadata filesize to match the updated data written to the file
            printf("Filesize now is: %d\n", dir->entry[file].fileSize);
            bufLength = bufLength - n; //calculate how much more data to write
            if ((offset + n) == BLOCK_SIZE){ //if the data written now just filled up the data block
                //find a new data block to write in
                int freeBlock = nextFreeBlock(fat); //look for available data in FAT
                fat->file[startingBlock].next = freeBlock; //update the current last data block chain to have a next block
                fat->file[freeBlock].busy = 1; //set the next block to be busy
                fat->file[freeBlock].next = -1; //set the next block to be last
                startingBlock = freeBlock; //set the working data block to be the next block
            }
        }
    }
    //since the file has been modified, update the date and time
    getTime(dir->entry[file].time);
    getDate(dir->entry[file].date);
    memcpy(&(data->blocks[peek()]), dir, sizeof(*dir));
}

//within the data block chain, find the last data block and the offset into it
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

//look for the specific file in the current directory
//basically, open file
int findFileEntry(directory * dir, char * filename, char * ext){
    int i;
    for (i = 0; i < MAX_ENTRIES; i++){
        if (((strcmp(dir->entry[i].filename, filename)) == 0) && //looks for same filename
                ((strcmp(dir->entry[i].extension, ext)) == 0) && //looks for same extension
                (dir->entry[i].folder == 0) && //looks if it is not a folder
                (dir->entry[i].startingIndex != 0)){ //looks if the entry is a deleted file
            return i;
        }
    }
    return -1;
}

//delete the file
void deleteFile(FAT * fat, DATA * data, char * filename, char * ext){
    //load the current directory
    directory *dir = malloc(sizeof(*dir));
    memcpy(dir, &(data->blocks[peek()]), sizeof(*dir));

    //open the file
    int i = findFileEntry(dir, filename, ext);
    if (i == -1){ //file open failure
        printf("Can't find file!\n");
        return;
    } else{ //file open success
        int block = dir->entry[i].startingIndex; //starting index in the FAT
        //go through the data block chain to
        //set busy as 0 and next as -1
        while (fat->file[block].next != -1){ 
            int temp = fat->file[block].next;
            fat->file[block].busy = 0;
            fat->file[block].next = -1;
            block = temp;
        }
        fat->file[block].busy = 0;
        fat->file[block].next = -1;
        //finally, edit the metadata to indicate empty entry
        //there is no need to zero out the actual data
        //since writeToFile will overwrite the data location
        editFileSize(&dir->entry[i].fileSize, 0);
        editIndex(&dir->entry[i].startingIndex, 0);
    } 

    memcpy(&(data->blocks[peek()]), dir, sizeof(*dir));
}

//print out the file
void readFile(FAT * fat, DATA * data, char * filename, char * ext){
    //load the current directory
    directory *dir = malloc(sizeof(*dir));
    memcpy(dir, &(data->blocks[peek()]), sizeof(*dir));

    //open the file
    int file = findFileEntry(dir, filename, ext);
    if (file == -1){ //file open failure
        printf("Can't find file!\n");
    } else{ //file open success
        char buf[dir->entry[file].fileSize + 1]; //create a buffer large enough to hold the entire file data
        memset(buf, '\0', sizeof(buf)); //null-terminate the whole thing

        int startBlock = dir->entry[file].startingIndex; //find the start block
        char temp[BLOCK_SIZE + 1]; //this temp block will contain each block's data and dump it to buffer

        //basically goes through the data block chain until there is no more
        while(fat->file[startBlock].next != -1){ 
            memset(temp, '\0', sizeof(temp));
            memcpy(temp, data->blocks[startBlock].sect, BLOCK_SIZE); 
            strcat(buf, temp);
            startBlock = fat->file[startBlock].next;
        }
        //do the same thing for the last data block
        int offset = findFileOffset(fat, data, dir, file);
        memset(temp, '\0', sizeof(temp));
        memcpy(temp, data->blocks[startBlock].sect, offset);
        strcat(buf, temp);

        //print the buffer to the stdout
        printf("%s\n", buf);
    }
    memcpy(&(data->blocks[peek()]), dir, sizeof(*dir));
}

/*
 * these are stack functions to assist in navigating through the directories in the filesystem
 */
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

//change directory for this filesystem
int cd(DATA * data, char * dirName){
    //go to previous directory
    if (strcmp(dirName, "..") == 0){
        pop();
        return 1;
    }

    //load current directory
    directory * dir = malloc(sizeof(*dir));
    memcpy(dir, &(data->blocks[peek()]), sizeof(*dir));

    int i;
    for (i = 0; i < MAX_ENTRIES; i++){
        if ((strcmp(dirName, dir->entry[i].filename) == 0) && (dir->entry[i].folder == 1)){ //if the subdirectory is found
            push(dir->entry[i].startingIndex); //go to that directory, indicated by pushing to the stack
            return 1;
        } 
    }
    return 0;
}


