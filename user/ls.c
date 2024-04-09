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

void
ls(char *path)
{
	char buf[512], *p;
	int file_descriptor;
	struct dirent directory_entry;
	struct stat stat_struct;
									// set the onofollow flag here
	if((file_descriptor = open(path, 0x004)) < 0){
		fprintf(2, "ls: cannot open %s\n", path);
		return;
	}

	if(fstat(file_descriptor, &stat_struct) < 0){
		fprintf(2, "ls: cannot stat %s\n", path);
		close(file_descriptor);
		return;
	}

	switch(stat_struct.type){
	case T_FILE:
		printf("%s %d %d %d %d\n", fmtname(path), stat_struct.type, stat_struct.ino, stat_struct.size, stat_struct.block);
		break;

	case T_DIR:
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
			if (stat_struct.type == T_SYMLINK){
				char dest[DIRSIZ];
                if (get_symlink_data(buf, dest, 0) == 0){
					fprintf(2, "symlinkinfo: cannot read %s\n", (path+2));
				}
				else {
					printf("%s %d %d %d %d -> %s\n", fmtname(buf), stat_struct.type, stat_struct.ino, stat_struct.size, stat_struct.block, dest);
				}	
			}
			else {
				printf("%s %d %d %d %d\n", fmtname(buf), stat_struct.type, stat_struct.ino, stat_struct.size, stat_struct.block);
			}
			
		}
		break;

	case T_SYMLINK:
		printf("ENTERED GERE\n");
		char dest[DIRSIZ];
        get_symlink_data("", dest, file_descriptor);  
		printf("%s %d %d %d %d -> %s\n", fmtname(buf), stat_struct.type, stat_struct.ino, stat_struct.size, stat_struct.block, dest);
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
	exit();
}
