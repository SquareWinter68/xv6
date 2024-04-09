#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/fcntl.h"
#include "user.h"
#include "kernel/x86.h"
#include "kernel/fs.h"

char*
strcpy(char *s, const char *t)
{
	char *os;

	os = s;
	while((*s++ = *t++) != 0)
		;
	return os;
}

char*
strncpy(char *s, const char *t, int n)
{
	char *os;

	os = s;
	while(n-- > 0 && (*s++ = *t++) != 0)
		;
	while(n-- > 0)
		*s++ = 0;
	return os;
}

// Like strncpy but guaranteed to NUL-terminate.
char*
safestrcpy(char *s, const char *t, int n)
{
	char *os;

	os = s;
	if(n <= 0)
		return os;
	while(--n > 0 && (*s++ = *t++) != 0)
		;
	*s = 0;
	return os;
}

int
strcmp(const char *p, const char *q)
{
	while(*p && *p == *q)
		p++, q++;
	return (uchar)*p - (uchar)*q;
}

uint
strlen(const char *s)
{
	int n;

	for(n = 0; s[n]; n++)
		;
	return n;
}

void*
memset(void *dst, int c, uint n)
{
	stosb(dst, c, n);
	return dst;
}

char*
strchr(const char *s, char c)
{
	for(; *s; s++)
		if(*s == c)
			return (char*)s;
	return 0;
}

char*
gets(char *buf, int max)
{
	int i, cc;
	char c;

	for(i=0; i+1 < max; ){
		cc = read(0, &c, 1);
		if(cc < 1)
			break;
		buf[i++] = c;
		if(c == '\n' || c == '\r')
			break;
	}
	buf[i] = '\0';
	return buf;
}

int
stat(const char *n, struct stat *st)
{
	int fd;
	int r;

	fd = open(n, (O_NOFOLLOW)); //replaced O_readonly with nofollow, because of symlink
	if(fd < 0)
		return -1;
	r = fstat(fd, st);
	close(fd);
	return r;
}

int
atoi(const char *s)
{
	int n;

	n = 0;
	while('0' <= *s && *s <= '9')
		n = n*10 + *s++ - '0';
	return n;
}

void*
memmove(void *vdst, const void *vsrc, int n)
{
	char *dst;
	const char *src;

	dst = vdst;
	src = vsrc;
	while(n-- > 0)
		*dst++ = *src++;
	return vdst;
}

int get_symlink_data(char* path, char* destination, int passed_file_descriptor){
    // this function will copy the data stored in the inode of the symlink to the destinaion buffer
    //char buffer[DIRSIZ];
    
    // Even though the 0 file descriptor is by convention used for stdin, here it will be used
    // treated as null, aka like it was not sent at all
    int file_descriptor;
    int read_sucess;
    if (passed_file_descriptor){
        if ((read_sucess = read(passed_file_descriptor, destination, DIRSIZ)) <= 0){
            //fprintf(2, "symlinkinfo: cannot read %s\n", (path+2));
            return 0;
        }
        return 1;
    }
    else {
        if((file_descriptor = open(path, 0x004)) < 0){
		    //fprintf(2, "symlinkinfo: cannot open %s\n", (path+2));
		    return 0;
	    }
        //int read(int, void*, int);
        // read(descriptor, buffer, bytes)
        memset(destination, 0, DIRSIZ);
        if ((read_sucess = read(file_descriptor, destination, DIRSIZ)) <= 0){
            //fprintf(2, "symlinkinfo: cannot read %s\n", (path+2));
            return 0;
        }
        return 1;
    }
    
}