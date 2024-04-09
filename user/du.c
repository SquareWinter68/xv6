#include "kernel/types.h"
#include "kernel/stat.h"
#include "user.h"
#include "kernel/fs.h"


char*
fmtname(char *path)
{
	static char buf[DIRSIZ+1];
	char *p;

	// Find first character after last slash.
	for(p=path+strlen(path); p >= path && *p != '/'; p--)
		;
	p++;

	// Return blank-padded name.
	if(strlen(p) >= DIRSIZ)
		return p;
	memmove(buf, p, strlen(p));
	memset(buf+strlen(p), ' ', DIRSIZ-strlen(p));
	return buf;
}

int read_directory_entry(char* path, struct stat* stat_struct){

}

void
du(char *path, int* total_blocks)
{
	char buf[512], *p;
	int file_descriptor;
	struct dirent directory_entry;
	struct stat stat_struct;
    // checks if file desctiptor is greater than 0, because xv6 treats -1 as error code
    // aduo in unix systems directories alont with pretty much everything else have file descriptors
    // so open syscall can indeed be called with a dir as an argument
	if((file_descriptor = open(path, 0)) < 0){
		fprintf(2, "du: cannot open %s\n", path);
		return;
	}
    // same story as above
    // Fstat loads the files metadata into the stat_struct structure
    // fstat is located in kernel/sysfile.c/sys_fstat
    // sys_fstat then calls file_stat located in kernel/file.c which then calls
    // stati in kernel/fs.c
    // fstat -> sys_fstat -> file_stat -> stati
	if(fstat(file_descriptor, &stat_struct) < 0){
		fprintf(2, "du: cannot stat_struct %s\n", path);
		close(file_descriptor);
		return;
	}

	switch(stat_struct.type){
	case T_FILE:
    // BSIZE corresponds to the block size of 512 bytes, defined in fs.h
        (*total_blocks) += stat_struct.block;
		printf("%s %d\n", fmtname(buf), stat_struct.block);
		break;

	case T_DIR:
    // xv6 has a fixed number of characters thah a dir name can be, it is 14 in this case
    // This is what the DIRSIZE refers to

	// ignore .. for size, but keep .
		if(strlen(path) + 1 + DIRSIZ + 1 > sizeof buf){
			printf("du: path too long\n");
			break;
		}
		strcpy(buf, path);
		p = buf+strlen(buf);
		*p++ = '/';
		while(read(file_descriptor, &directory_entry, sizeof(directory_entry)) == sizeof(directory_entry)){
			// if the dirent points to anothet directory, the du is called once again with the path of the new directory
			if(directory_entry.inum == 0)
				continue;
			memmove(p, directory_entry.name, DIRSIZ);
			p[DIRSIZ] = 0;
			if(stat(buf, &stat_struct) < 0){
				printf("du: cannot stat %s\n", buf);
				continue;
			}
			if (stat_struct.type == T_DIR && strcmp((buf+strlen(buf)-1), ".") != 0 && strcmp((buf+strlen(buf)-2), "..") != 0){
				// ignoring . and .. directories
				//printf("the path of the currnet dir is %s\n", buf);
				du(buf, total_blocks);
			}
			if (strcmp((buf+strlen(buf)-2), "..") != 0){
				(*total_blocks) += stat_struct.block;
				printf("%s %d\n", fmtname(buf), stat_struct.block);
			}
            
		}
		break;
	}
	close(file_descriptor);
}

int
main(int argc, char *argv[])
{
	int total = 0;

	int i;

	if(argc < 2){
		du(".", &total);
		fprintf(0,"total: %d\n", total);
		exit();
	}
	for(i=1; i<argc; i++)
		du(argv[i], &total);
    fprintf(0,"total: %d\n", total);
	exit();
}
