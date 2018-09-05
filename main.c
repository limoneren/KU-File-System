#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#define BLOCK_SIZE 1024
#define FILE_NAME_SIZE 100
#define BLOCK_LIST_SIZE 100
int * ptr;
int *block_states;
int disk_sz;
char* disk_nm;
int entries_size = 0;
int opened_file_table_size = 0;
int *opened_file_table;
int next_id = 0;

//functions
int parseFAT(char* buffer);
int findEntry(char* file_name);
int parseBlockList(int fd_index, int * block_list);

typedef struct fat
{
    
} fat;
// fat disk_node;

typedef struct fat_entry
{
    char *file_size;
    char *file_name;
    char *block_list;
    int file_position;
    int fd;
    int seek_flag;
} fat_entry;
fat_entry* entries;

int kufs_delete(char* file_name)
{
    int ind = findEntry(file_name);
    if (ind == -1){
        printf("File not found: %s\n", file_name);
        return -1;
    }
    
    /*record the blocks occupied*/
    char buffer[2*disk_sz];
    strncpy(buffer,entries[ind].block_list,strlen(buffer));
    /*shift the entries*/
    for (int i = ind; i < entries_size; i++){
        if (i < entries_size-1 ) {
            entries[i].file_name = entries[i+1].file_name;
            entries[i].file_size = entries[i+1].file_size;
            entries[i].block_list = entries[i+1].block_list;
            entries[i].file_position = entries[i+1].file_position;
        }
    }
    /*decrement the size*/
    entries_size--;
    
    /*parse the block list*/
    char *token;
    const char s[2] = ",";
    
    /* get the first block */
    token = strtok(buffer, s);
    
    printf("Parse the block list string.\n");
    /* invalidate blocks*/
    while( token != NULL ) {
        printf( "Token:%s\n", token );
        block_states[atoi(token)] = 0;
        token = strtok(NULL,s);
    }
    printf("File %s is deleted.\n", file_name );
    return 0;
}

int findEntry(char* file_name)
{
    for (int i = 0; i < entries_size; i++){
        if (strcmp(entries[i].file_name, file_name) == 0) {
            return i;
        }
    }
    return -1;
}

int kufs_mount(char* disk_name)
{
    /*initialize entries data structure*/
    entries = (fat_entry*) malloc(disk_sz * sizeof(fat_entry));
    /*initialize (un)allocated blocks array*/
    block_states = (int*) malloc(disk_sz * sizeof(int));
    for (int i = 0; i < disk_sz; i++) {
        block_states[i] = 0;
    }
    /*initialize opened file table*/
    opened_file_table = (int*) malloc(disk_sz * sizeof(int));
    /*create a buffer for the first block*/
    char buffer[BLOCK_SIZE];
    FILE* fp = fopen(disk_name, "w");
    /*seek the beginning of the file*/
    fseek(fp, 0, SEEK_SET);
    /*read the content to the buffer*/
    fread(buffer, BLOCK_SIZE+1, 1, fp);
    /*close FAT*/
    fclose(fp);
    /*fill up the entries data structure*/
    parseFAT(buffer);
    return 0;
}



int kufs_umount()
{
    /*arrays of unallocated blocks and opened files kufs_create_disk*/
    free(block_states);
    free(opened_file_table);
    printf("block_states, opened_file_table and deleted_files freed.\n");
    
    /*overwrite the FAT*/
    FILE *fp = fopen(disk_nm, "r+b");
    char str[BLOCK_SIZE];
    for (int i = 0; i < entries_size; i++) {
        strcat(str, entries[i].file_name);
        strcat(str, " ");
        strcat(str, entries[i].file_size);
        strcat(str, " ");
        strcat(str, entries[i].block_list);
        strcat(str, " ");
    }
    
    printf("pointer: %d",fseek(fp,0, SEEK_SET));
    fwrite(str , 1 , sizeof(str) , fp );
    fclose(fp);
    printf("FAT is written back to the first block.\n");
    
    /*FAT freed*/
    free(entries);
    printf("Entries freed.\n");
    
    return 0;
}

char* zero = "0";


int kufs_create(char* filename)
{
    int i;
    int flag = 0;
    //check if a file with the same name exists
    for(i = 0; i < entries_size ; i++){
        //printf("\n%s\n",entries[i].file_name);
        if(strcmp(entries[i].file_name,filename) == 0){
            printf("same file exists!\n");
            flag = 1;
        }
    }
    if(flag == 0){
        entries[entries_size].file_size = zero;
        entries[entries_size].file_name = filename;
        entries[entries_size].block_list = malloc(BLOCK_SIZE*2);
        entries[entries_size].block_list = " ";
        entries[entries_size].file_position = -1;
        entries[entries_size].fd = next_id++;
        entries[entries_size].seek_flag = 0;
        entries_size++;
    }
    return 0;
}

int kufs_open(char* filename)
{
    int fd = -1;
    for(int i = 0; i < entries_size; i++){
        if(strcmp(entries[i].file_name,filename) == 0){
            fd = entries[i].fd;
            //printf("(kufs_open debugging)entries[i].file_position:  %s\n", entries[i].file_position);
            int block_list[disk_sz];
            parseBlockList(fd, block_list);
            printf("(kufs_open debugging)entries[fd_index].block_list:  %s\n", entries[fd].block_list);
            printf("(kufs_open debugging)entries[fd_index].block_list LENGTH:  %d\n", strlen(entries[fd].block_list));
            
            
            if(strlen(entries[fd].block_list) == 1){ // when lengths is 1, it is only '\0'
                entries[i].file_position = -1;
            } else { // file is opened again
                int first_allocated = (entries[fd].block_list[0]-'0');
                printf("(kufs_open debugging)entries[fd_index].block_list[0]:  %d\n", first_allocated);
                entries[i].file_position = (entries[fd].block_list[0]-'0') * BLOCK_SIZE - 1;
                block_states[first_allocated] = 0; // so that kufs_write can start writing from it
                entries[i].block_list = " ";
            }
            
        }
    }
    if(fd != -1){
        opened_file_table[opened_file_table_size] = fd;
        opened_file_table_size++;
        
    
        
        
    } else {
        printf("Couldn't find %s\n", filename);
    }
    return fd;
}

int kufs_close(int fd)
{
    int found = -1;
    for (int i = 0; i < opened_file_table_size; i++) {
        if (opened_file_table[i] == fd) {
            found = fd;
            for(int j = i; j < opened_file_table_size; j++){
                opened_file_table[j] = opened_file_table[j+1];
            }
            opened_file_table_size--;
        }
    }
    return found;
}

int print_opened_table()
{
    int i;
    for(i = 0;i < opened_file_table_size; i++){
        printf("%d ",opened_file_table[i]);
    }
    printf("\n");
    return 0;
}

int parseFAT(char* buffer)
{
    int i = 0;
    char *token;
    const char s[2] = " ";
    
    /* get the first token */
    token = strtok(buffer, s);
    printf("Read the content to entries.\n");
    
    /* walk through other tokens */
    while( token != NULL ) {
        printf( "Token:%s\n", token );
        
        /***** read each entry to entries ****/
        // token2 = strtok(token, s2);
        int f = (int)i/3;
        // printf("FILE: %d\n",f );
        if (i%3 == 0) {
            // printf("File name : %s\n", token);
            entries[f].file_name = token;
        } else if (i%3 == 1) {
            // printf("File size: %s\n",token);
            entries[f].file_size = token;
        }else {
            // printf("File blocks:%s\n",token);
            entries[f].block_list = token;
        }
        /**********************************/
        token=strtok(NULL, s);
        i++;
    }
    return 0;
}


int kufs_create_disk(char* disk_name, int disk_size)
{
    disk_sz = disk_size;
    disk_nm = disk_name;
    FILE * disk = fopen(disk_name, "w");
    // printf("Disk num: %d\n", (int)disk );
    if (disk == NULL) {
        return -1;
    }
    fseek(disk, BLOCK_SIZE * disk_size, SEEK_SET);
    fputc('\0', disk);
    fclose(disk);
    return 0;
}

int return_fd_index(int fd)
{
    int index = -1;
    for (int i = 0; i < entries_size; i++) {
        if(entries[i].fd == fd){
            index = i;
            break;
        }
    }
    return index;
}

int next_available_block = 1;

int find_next_available_block()
{
    int i;
    for(i = 1; i < disk_sz; i++){
        //int index = sizeof(block_states) / sizeof(block_states[0]);
        //printf("\nNext avaiable block at: %d\n",index);
        if(block_states[i] == 0){
            return i;
        }
    }
    printf("Blocks are full! Do not attempt to write more!");
    return -999;
}

void kufs_dump_fat()
{
    printf("Dumping starts.\n");
    for (int i = 0; i < entries_size; i++) {
        printf("%s: %s\n",entries[i].file_name, entries[i].block_list);
    }
    printf("Dumping ends.\n");
}

int find_last_unfull_block(int fd){
    int index = -1;
    //int fd_index = return_fd_index(fd);
    char buf[10];
    int i = 0;
    char *last = strrchr(entries[fd].block_list,' ');
    
    if (last != NULL) {
        last--;
        while(*last != ' '){
            last--;
        }
        last++;
        while(*last != ' '){
            buf[i] = *last;
            //printf("Last token: %d '%c'\n", i, buf[i]);
            i++;
            last++;
        }
        buf[i] = '\0';
        //printf("Last token: %d\n",atoi(buf));
    }
    
    return atoi(buf);;
}

int kufs_read(int fd, void* buf, int n){
    int fd_index = return_fd_index(fd);
    FILE *f = fopen(disk_nm, "r+");
    int fp = entries[fd_index].file_position+1;
    printf("file position in reading: %d\n",fp);
    int block_list[disk_sz];
    int numBlocks = fp/BLOCK_SIZE + 1;
    int filesize = atoi(entries[fd_index].file_size);
    int cum_read = 0;
    
    parseBlockList(fd_index, block_list);
    printf("entries[fd_index].block_list:  %s\n", entries[fd_index].block_list);
    
    //if n is bigger than the file size read file size only
    int remaining = n;
    
    //read the first buffer
    // char buffer[n];
    fseek(f,fp, SEEK_SET);
    int read_size = numBlocks*BLOCK_SIZE - fp;
    printf("Read size: %d\n", read_size);
    fread(buf,1,read_size,f);
    // strcat(buf,buffer);
    remaining -= read_size;
    printf("BUF%s\n",buf );
    printf("numBlocks: %d\n", numBlocks);
    cum_read += read_size;
    
    for (int i = 1; i < numBlocks; i++) {
        // char buffer2[n];
        fp = BLOCK_SIZE * block_list[i];
        printf("FP in read: %d\n", fp);
        read_size = remaining > BLOCK_SIZE ? BLOCK_SIZE : remaining;
        printf("readsize: %d\n", read_size);
        fseek(f,fp,SEEK_SET);
        fread(buf+cum_read, 1,remaining, f);
        printf("BUFFER2%s\n",buf );
        // strcat(buf,buffer2);
        cum_read += read_size;
        remaining -= read_size;
    }
    
    printf("buffer after read: %s\n",buf);
    
    fclose(f);
    return 0;
}

int kufs_write(int fd, void* buf, int n)
{
    int i,k;
    int is_opened = 0;
    int fd_index = return_fd_index(fd);
    printf("fd: %d, fd_index: %d\n", fd, fd_index);
    int cum_written = 0;
    /*check whether the file is open*/
    int write_size = n;
    for(i = 0; i < opened_file_table_size; i++){
        if(opened_file_table[i] == fd){
            is_opened = 1;
            break;
        }
    }
    
    int avail;
    int block_list[disk_sz];
    int avail_block = find_next_available_block();
    
    if (is_opened != 1) {
        printf("File %d should be open to write.\n", fd);
        return -1;
    }
    
    /*open the disk to write*/
    FILE *disk = fopen(disk_nm,"r+");
    
    int file_p = entries[fd_index].file_position;
    
    int numBlocks;
    
    printf("String to be written: %s\n", buf);
    printf("File position: %d\n", file_p);
    printf("Chars to be written: %d\n", write_size);
    
    // printf("Zeroth element of the block list parsed: %d\n", block_list[0]);
    
    while (write_size > 0) {
        numBlocks = parseBlockList(fd_index, block_list);
        printf("Size of the block list parsed: %d\n", numBlocks);
        if ((file_p + 1) % BLOCK_SIZE == 0 || file_p == -1) {
            /* will write to another block*/
            printf("Will write to another block\n");
            printf("Avail block: %d\n", avail_block);
            
            /*SIMDI BLOCK LIST DOLUYSA ONU TAKIP ETMELI
             AVAIL_BLOCK ONA GORE BELIRLENCEK.*/
            if(entries[fd_index].seek_flag == 1){
                avail_block = block_list[numBlocks-1];
            }
            
            
            file_p = avail_block * BLOCK_SIZE;
            avail = BLOCK_SIZE;
            /* add the avail block to block_list */
            printf("Block list to be updated\n");
            
            char *block_string; //[100];
            
            if (numBlocks > 0) {
                printf("NUMBLOCK: %d\n",numBlocks );
                block_string = (char*) malloc(30 * sizeof(char));
                for (int i = 0; i < numBlocks; i++) {
                    char num_c[3];
                    
                    sprintf(num_c, "%d", block_list[i]);
                    //printf("(!!!!!)num_c: %s\n", num_c);
                    
                    
                    
                    strcat(block_string, num_c);
                    strcat(block_string, ",");
                    //printf("(!!!!!!!)block_string: %s\n", block_string);
                    
                    
                }
                if(entries[fd_index].seek_flag != 1){
                    char str[3];
                    sprintf(str, "%d", avail_block);
                    //printf("(!!!!!!!)avail_block: %d\n", avail_block);
                    
                    strcat(block_string, str);
                    strcat(block_string, ",");
                }
                
            } else {
                block_string = (char*) malloc(30 * sizeof(char));
                char str[3];
                sprintf(str, "%d", avail_block);
                printf("STRRRRRRR:%s\n", str);
                strcat(block_string, str);
                strcat(block_string, ",");
                printf("BLOCKSTRING:%s\n", block_string);
            }
            
            /*update the block list in entries*/
            entries[fd_index].block_list = block_string;
            
            // sprintf(entries[fd_index].block_list + strlen(entries[fd_index].block_list),"%d,",avail_block);
            printf("After update block list: %s\n", entries[fd_index].block_list);
            // entries[fd_index].block_list = block_list;
            // printf("write for %d: block_list %s", fd, entries[fd_index].block_list);
            block_states[avail_block] = 1;
        } else {
            file_p++;
            avail = (file_p / BLOCK_SIZE + 1) * BLOCK_SIZE - file_p;
            printf("Use the half filled block. Availability: %d\n", avail);
        }
        printf("File position to be written: %d\n", file_p);
        fseek(disk, file_p, SEEK_SET);
        int min = write_size < avail ? write_size : avail;
        printf("Min: %d\n",min );
        
        fwrite(buf + cum_written,1, min, disk);
        cum_written += min;
        file_p = file_p + min - 1;
        printf("After write file position: %d\n", file_p);
        entries[fd_index].file_position = file_p;
        avail_block = find_next_available_block();
        write_size -= min;
        printf("New write size: %d\n",write_size );
    }
    printf("ONCE SIZE: %s\n", entries[fd_index].file_size);
    int num = atoi(entries[fd_index].file_size) + n;
    char mybuf[10];
    sprintf(mybuf, "%d", num);
    entries[fd_index].file_size = &mybuf[0];
    // sprintf(entries[fd_index].file_size, "%d", num+ n);
    printf("SONRA SIZE: %s\n", entries[fd_index].file_size);
    fclose(disk);
    return 0;
}

int parseBlockList(int fd_index, int * block_list)
{
    int i = 0;
    char *token;
    const char s[2] = ",";
    
    /* get the first token */
    // printf("parse: %s\n", entries[fd_index].block_list);
    printf("parse blocklist:%s\n", entries[fd_index].block_list);
    char buf[30];
    strcpy(buf,entries[fd_index].block_list);
    token = strtok(buf, s);
    printf("parse blocklist2:%s\n", entries[fd_index].block_list);
    printf("Parse the block_list.\n");
    
    /* walk through other tokens */
    while( token != NULL ) {
        if (strcmp(token, " ") == 0) {
            break;
        }
        // printf( "Blockish:%s\n", token );
        block_list[i] = atoi(token);
        token=strtok(NULL, s);
        i++;
    }
    return i;
}

// int find_first_block(){
//
//   int index = -1;
//   //int fd_index = return_fd_index(fd);
//   char buf[10];
//   int i = 0;
//   char *last = entries[fd].block_list;
//
//   if (last != NULL) {
//       while(*last != ','){
//         buf[i] = *last;
//         last++;
//         i++;
//       }
//       buf[i] = '\0';
//       printf("Last token: %d\n",atoi(buf));
//     }
//
//   return atoi(buf);;
//
// }

int kufs_seek(int fd, int n){
    n--;
    int fd_index = return_fd_index(fd);
    // if(n > atoi(entries[fd_index].file_size)){
    //   return atoi(entries[fd_index].file_size);
    // }
    printf("INSEEK%d\n", atoi(entries[fd_index].file_size));
    if(atoi(entries[fd_index].file_size) < n) {
        char buffer[10];
        sprintf(buffer, "%d", n+1);
        entries[fd_index].file_size = &buffer[0];
    }
    
    int block_list[disk_sz];
    int numBlocks = parseBlockList(fd_index, block_list);
    printf("seek:Num blocks: %d", numBlocks);
    int divident = n/BLOCK_SIZE;
    int remaining = n % BLOCK_SIZE;
    printf("seek:divident: %d, remaing: %d", numBlocks, remaining);
    int file_p = block_list[divident] * BLOCK_SIZE + remaining;
    entries[fd_index].seek_flag = 1;
    entries[fd_index].file_position = file_p;
    return file_p;
    
}

int main(void)
{
    // printf("%d\n", kufs_create_disk("deneme", 1));
    
    char * disk_name = "test.txt";
    char readbuffer[10];
    char sampleb[100];
    char samplea[1300];
    int i = 0;
    
    for(i = 0; i < 1300; i++){
        if(i == 1299){
            samplea[i] = '\0';
        } else {
            samplea[i] = 'a';
        }
    }
    i = 0;
    for(i = 0; i < 100; i++){
        if(i == 99){
            sampleb[i] = '\0';
        } else {
            sampleb[i] = 'b';
        }
    }
    
    //printf("test: %s\n",sample);
    //sleep(2);
    
    /* creating a disk with 5 blocks */
    kufs_create_disk(disk_name, 5);
    
    /* mounting the disk */
    kufs_mount(disk_name);
    
    /* creating 3 files which are supposed to be
     added to fat data structure */
    kufs_create("file1");
    kufs_create("file2");
    kufs_create("file3");
    
    /* checking the content of fat up to this point, it should show all the 3 files without block ids */
    kufs_dump_fat();
    
    // open file2
    int fd = kufs_open("file2");
    
    /* checking if file descriptor matches the position
     of file entry in fat data structure */
    printf("fd: %d\n", fd);
    
    /* Following 2 lines write 2 strings to file2 such that the second string
     should be written right after the first string.
     Since the first written file is file2, the first block in virtual hard disk
     should be allocated to file2, i.e. these 2 strings must be written in the first block. */
    kufs_write(fd, "written string1", 15);
    kufs_write(fd, "written string2", 15);
    
    
    // close file2
    kufs_close(fd);
    // open file1
    fd = kufs_open("file1");
    
    /* The following line writes string to the block allocated to file1.
     Since file1 is written after file2, the second block in virtual hard disk
     should be allocated to file1. */
    kufs_write(fd, "the next written string", 15);
    
    char char_blocks[2048];
    
    for(i = 0; i < 2048; i++)
    {
        char_blocks[i] = 'a';
    }
    
    // close file1
    printf("Closed: %d\n", kufs_close(fd));
    
    // file2 is reopened again, therefore its file_position should be zero.
    fd = kufs_open("file2");
    
    /* file2 is written. The 2048 byte string is written into first block and third block in virtual hard disk,
     since first block is already allocated to file2,
     second block is allocated to file1 and third block is unallocated.*/
    kufs_write(fd, char_blocks, 2048);
    
    // file position is set to 1022
    kufs_seek(fd, 1022);
    
    /* the word hello should be divided such that 'he' is written at the end of first block
     and 'llo' is written at the beginning of the third block*/
    kufs_write(fd, "hello", 5);
    
    //file position is repositioned to 1022nd byte in file2 again.
    kufs_seek(fd, 1022);
    char read_chars[30];
    
    // 5 bytes are read from file2 starting from current file position
    kufs_read(fd, read_chars, 5);
    
    // following line should print 'hello'
    printf("read_chars: %s\n", read_chars);
    
    /* Content of FAT is checked again. First and third blocks should be shown in the same line as file2 */
    kufs_dump_fat();
    
    // close file2
    kufs_close(fd);
    
    // file 1 is deleted
    kufs_delete("file1");
    
    /* File system is unmounted.
     The first block in the file should no longer display entry for file1.*/
    kufs_umount();
    
    return 0;
    
    /* checking the content of fat up to this point, it should show all the 3 files without block ids */
    // kufs_dump_fat();
    //
    // // open file2
    // int fd = kufs_open("file2");
    //
    // /* checking if file descriptor matches the position
    // of file entry in fat data structure */
    // printf("fd: %d\n", fd);
    // // kufs_delete("file1");
    // kufs_dump_fat();
    //
    // // sprintf(sample,"%s","hello");
    // kufs_write(fd,sample2,sizeof(sample2));
    // printf("kufs_write is over\n");
    //
    // kufs_read(fd,readbuffer,10);
    // printf("(in main)buffer after read: %s %d\n",readbuffer,sizeof(readbuffer));
    //
    // kufs_umount();
}
