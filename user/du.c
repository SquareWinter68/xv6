#include "kernel/types.h"
#include "kernel/stat.h"
#include "user.h"
#include "kernel/fs.h"

int total = 0;

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

void
ls(char *path)
{
	char buf[512], *p;
	int file_descriptor;
	struct dirent directory_entry;
	struct stat stat_struct;
    // checks if file desctiptor is greater than 0, because xv6 treats -1 as error code
    // also in unix systems directories alont with pretty much everything else have file descriptors
    // so open syscall can indeed be called with a dir as an argument
	if((file_descriptor = open(path, 0)) < 0){
		fprintf(2, "ls: cannot open %s\n", path);
		return;
	}
    // same story as above
    // Fstat loads the files metadata into the stat_struct structure
    // fstat is located in kernel/sysfile.c/sys_fstat
    // sys_fstat then calls file_stat located in kernel/file.c which then calls
    // stati in kernel/fs.c
    // fstat -> sys_fstat -> file_stat -> stati
	if(fstat(file_descriptor, &stat_struct) < 0){
		fprintf(2, "ls: cannot stat_struct %s\n", path);
		close(file_descriptor);
		return;
	}

	switch(stat_struct.type){
	case T_FILE:
    // BSIZE corresponds to the block size of 512 bytes, defined in fs.h
        total += ((stat_struct.size/BSIZE)+1);
		printf("%s %d %d %d\n", fmtname(path), stat_struct.type, stat_struct.ino, (stat_struct.size/BSIZE)+1);
		break;

	case T_DIR:
    // xv6 has a fixed number of characters thah a dir name can be, it is 14 in this case
    // This is what the DIRSIZE refers to
		if(strlen(path) + 1 + DIRSIZ + 1 > sizeof buf){
			printf("ls: path too long\n");
			break;
		}
		strcpy(buf, path);
		p = buf+strlen(buf);
		*p++ = '/';
		while(read(file_descriptor, &directory_entry, sizeof(directory_entry)) == sizeof(directory_entry)){
			if(directory_entry.inum == 0)
				continue;
			memmove(p, directory_entry.name, DIRSIZ);
			p[DIRSIZ] = 0;
			if(stat(buf, &stat_struct) < 0){
				printf("ls: cannot stat %s\n", buf);
				continue;
			}
            total += ((stat_struct.size/BSIZE)+1);
			printf("%s %d %d %d\n", fmtname(buf), stat_struct.type, stat_struct.ino, (stat_struct.size/BSIZE)+1);
		}
		break;
	}
	close(file_descriptor);
}

int
main(int argc, char *argv[])
{

	int i;

	if(argc < 2){
		ls(".");
		exit();
	}
	for(i=1; i<argc; i++)
		ls(argv[i]);
    printf("total: %d\n", total);
	exit();
}
