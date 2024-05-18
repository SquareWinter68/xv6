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

// int get_symlink_data(char* path, char* destination, int passed_file_descriptor){
//     // this function will store the data in the inode of the symlink to the destinaion buffer
//     //char buffer[DIRSIZ];
    
//     // Even though the 0 file descriptor is by convention used for stdin, here it will be used
//     // treated as null, aka like it was not sent at all
//     int file_descriptor;
//     int read_sucess;
//     if (passed_file_descriptor){
//         if ((read_sucess = read(passed_file_descriptor, destination, DIRSIZ)) <= 0){
//             //fprintf(2, "symlinkinfo: cannot read %s\n", (path+2));
//             return 0;
//         }
//         return 1;
//     }
//     else {
//         if((file_descriptor = open(path, 0x004)) < 0){
// 		    fprintf(2, "symlinkinfo: cannot open %s\n", (path+2));
// 		    return 0;
// 	    }
//         //int read(int, void*, int);
//         // read(descriptor, buffer, bytes)
//         memset(destination, 0, DIRSIZ);
//         if ((read_sucess = read(file_descriptor, destination, DIRSIZ)) <= 0){
//             //fprintf(2, "symlinkinfo: cannot read %s\n", (path+2));
//             return 0;
//         }
//         return 1;
//     }
    
// }

void
symlinkinfo(char *path)
{
	char buf[512], *p;
	int file_descriptor;
	struct dirent directory_entry;
	struct stat stat_struct;
									// set the onofollow flag here
	if((file_descriptor = open(path, 0x004)) < 0){
		fprintf(2, "symlinkinfo: cannot open %s\n", (path+2));
		return;
	}

	if(fstat(file_descriptor, &stat_struct) < 0){
		fprintf(2, "symlinkinfo: cannot stat %s\n", (path+2));
		close(file_descriptor);
		return;
	}

	switch(stat_struct.type){
	case T_SYMLINK:
		//printf("%s %d %d %d %d\n", fmtname(path), stat_struct.type, stat_struct.ino, stat_struct.size, (stat_struct.size/BSIZE)+1);
        char dest[DIRSIZ];
        get_symlink_data("", dest, file_descriptor);  
							// offset by two to avoid ./
		printf("%s -> %s\n", (path+2), dest);
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
                //printf("found a symlink yeayea!\n");
                char dest[DIRSIZ];
                if (get_symlink_data(buf, dest, 0) == 0){
					fprintf(2, "symlinkinfo: cannot read %s\n", (path+2));
				}
				else {
					printf("%s -> %s\n", (buf+2), dest);
				}
                    
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
	int* pointer;
	char* name = "Is the place to be";
	char* name1 = "Is the place to be1";
	char* name2 = "Is the place to be2";
	char* name3 = "Is the place to be3";
	char* name4 = "Is the place to be4";
	char* name5 = "Is the place to be5";
	char* name6 = "Is the place to be6";
	char* name7 = "Is the place to be7";
	char* name8 = "Is the place to be8";
	char* name9 = "Is the place to be9";
	char* name10 = "Is the place to be10";
	char* name11 = "Is the place to be11";
	char* name12 = "Is the place to be12";
	char* name13 = "Is the place to be13";
	char* name14 = "Is the place to be14";
	char* name15 = "Is the place to be15";
	char* name16 = "Is the place to be16";
	//int object_descriptor = shm_open(name);
	shm_open(name);
	shm_open(name1);
	shm_open(name2);
	shm_open(name3);
	shm_open(name4);
	shm_open(name5);
	shm_open(name6);
	shm_open(name7);
	shm_open(name8);
	shm_open(name9);
	shm_open(name10);
	shm_open(name11);
	shm_open(name12);
	shm_open(name13);
	shm_open(name14);
	shm_open(name15);
	shm_close(0);
	shm_close(1);
	shm_close(2);
	shm_close(3);
	shm_close(4);
	shm_close(5);
	shm_close(6);
	shm_close(7);
	
	printf("===========================================\n===========================================\n");
	//shm_trunc(object_descriptor, 400);
	//shm_trunc(object_descriptor, 500);
	//shm_close(object_descriptor);
	//shm_trunc(object_descriptor, 500);
	//shm_map(object_descriptor, (void **) &pointer, 0);
	pointer[0] = 23;
	pointer[1] = 42;
	printf("the pointer turned out to be %d\n", pointer[1]);
	int object_descriptor1 = shm_open(name);
	//printf("test: %d\n", object_descriptor);
	//shm_close(object_descriptor);
	int i;

	if(argc < 2){
		symlinkinfo(".");
		exit();
	}
	for(i=1; i<argc; i++)
		symlinkinfo(argv[i]);
	exit();
}
