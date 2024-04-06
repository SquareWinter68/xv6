// basically copy the implementation for case T_dir, no need for recursice callin
// you will loop through the dirents, and concatinate them to the current path
// not much inlike the cursed way in which the developers did it
// Then you will callopen on the concatinaded paths, this will return a file descriptor
// you can obtain the type of this file by stat, and it wil be loaded in the stat struct.
// you will read the data aka what the link points to from the inode by 

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

int get_symlink_data(char* path, char* destination, int passed_file_descriptor){
    // this function will store the data in the inode of the symlink to the destinaion buffer
    //char buffer[DIRSIZ];
    
    // Even though the 0 file descriptor is by convention used for stdin, here it will be used
    // treated as null, aka like it was not sent at all
    int file_descriptor;
    int read_sucess;
    if (passed_file_descriptor){
        if ((read_sucess = read(passed_file_descriptor, destination, DIRSIZ)) <= 0){
            fprintf(2, "symlinkinfo: cannot read %s\n", path);
            return 0;
        }
        return 1;
    }
    else {
        if((file_descriptor = open(path, 0x004)) < 0){
		    fprintf(2, "symlinkinfo: cannot open %s\n", path);
		    return 0;
	    }
        //int read(int, void*, int);
        // read(descriptor, buffer, bytes)
        memset(destination, 0, DIRSIZ);
        if ((read_sucess = read(file_descriptor, destination, DIRSIZ)) <= 0){
            fprintf(2, "symlinkinfo: cannot read %s\n", path);
            return 0;
        }
        return 1;
    }
    
}

void
symlinkinfo(char *path)
{
	char buf[512], *p;
	int file_descriptor;
	struct dirent directory_entry;
	struct stat stat_struct;
									// set the onofollow flag here
	if((file_descriptor = open(path, 0x004)) < 0){
		fprintf(2, "symlinkinfo: cannot open %s\n", path);
		return;
	}

	if(fstat(file_descriptor, &stat_struct) < 0){
		fprintf(2, "symlinkinfo: cannot stat %s\n", path);
		close(file_descriptor);
		return;
	}

	switch(stat_struct.type){
	case T_SYMLINK:
		//printf("%s %d %d %d %d\n", fmtname(path), stat_struct.type, stat_struct.ino, stat_struct.size, (stat_struct.size/BSIZE)+1);
        char dest[DIRSIZ];
        get_symlink_data("", dest, file_descriptor);  
		printf("%s -> %s\n", path, dest);
        break;

	case T_DIR:
    // if size of path/currentdir/ exceedx buffer raise error
		if(strlen(path) + 1 + DIRSIZ + 1 > sizeof buf){
			printf("symlinkinfo: path too long\n");
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
            // int new_fd;
            // if(is_symlink(path, directory_entry.name, &new_fd)){
            //     printf("found a symlink yeayea!\n");
            // }
			if(stat(buf, &stat_struct) < 0){
				printf("symlinkinfo: cannot stat %s\n", buf);
				continue;
			}
            if (stat_struct.type == 4){
                printf("found a symlink yeayea!\n");
                char dest[DIRSIZ];
                if (get_symlink_data(buf, dest, 0))
                    printf("%s -> %s\n", buf, dest);
            }
			//printf("%s %d %d %d %d\n", fmtname(buf), stat_struct.type, stat_struct.ino, stat_struct.size, (stat_struct.size/BSIZE)+1);
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
		symlinkinfo(".");
		exit();
	}
	for(i=1; i<argc; i++)
		symlinkinfo(argv[i]);
	exit();
}
