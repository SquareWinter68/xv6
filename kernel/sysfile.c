//
// File-system system calls.
// Mostly argument checking, since we don't trust
// user code, and calls into file.c and fs.c.
//

#include "types.h"
#include "defs.h"
#include "param.h"
#include "stat.h"
#include "mmu.h"
#include "proc.h"
#include "fs.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "file.h"
#include "fcntl.h"

static struct inode*
create(char *path, short type, short major, short minor);
// declaring create so that i can use it in sys_symlink which is written over create

// Fetch the nth word-sized system call argument as a file descriptor
// and return both the descriptor and the corresponding struct file.
static int
argfd(int n, int *pfd, struct file **pf)
{
	int fd;
	struct file *f;

	if(argint(n, &fd) < 0)
		return -1;
	if(fd < 0 || fd >= NOFILE || (f=myproc()->ofile[fd]) == 0)
		return -1;
	if(pfd)
		*pfd = fd;
	if(pf)
		*pf = f;
	return 0;
}

// Allocate a file descriptor for the given file.
// Takes over file reference from caller on success.
static int
fdalloc(struct file *f)
{
	int fd;
	struct proc *curproc = myproc();

	for(fd = 0; fd < NOFILE; fd++){
		if(curproc->ofile[fd] == 0){
			curproc->ofile[fd] = f;
			return fd;
		}
	}
	return -1;
}

int
sys_dup(void)
{
	struct file *f;
	int fd;

	if(argfd(0, 0, &f) < 0)
		return -1;
	if((fd=fdalloc(f)) < 0)
		return -1;
	filedup(f);
	return fd;
}

int
sys_read(void)
{
	struct file *f;
	int n;
	char *p;

	if(argfd(0, 0, &f) < 0 || argint(2, &n) < 0 || argptr(1, &p, n) < 0)
		return -1;
	return fileread(f, p, n);
}

int
sys_write(void)
{
	struct file *f;
	int n;
	char *p;

	if(argfd(0, 0, &f) < 0 || argint(2, &n) < 0 || argptr(1, &p, n) < 0)
		return -1;
	return filewrite(f, p, n);
}

int
sys_close(void)
{
	int fd;
	struct file *f;

	if(argfd(0, &fd, &f) < 0)
		return -1;
	myproc()->ofile[fd] = 0;
	fileclose(f);
	return 0;
}

int
sys_fstat(void)
{
	struct file *f;
	struct stat *st;

	if(argfd(0, 0, &f) < 0 || argptr(1, (void*)&st, sizeof(*st)) < 0)
		return -1;
	return filestat(f, st);
}

// Create the path new as a link to the same inode as old.
int
sys_link(void)
{
	char name[DIRSIZ], *new, *old;
	struct inode *dp, *ip;

	if(argstr(0, &old) < 0 || argstr(1, &new) < 0)
		return -1;
	// Popping names form the stack retun error if name cannot be found

	begin_op();
	// Checks wether the node (in this case file) the user wants to link to exists
	if((ip = namei(old)) == 0){
		end_op();
		return -1;
		// Does not exist, cannot link to non existant file
	}

	ilock(ip);
	if(ip->type == T_DIR){
		iunlockput(ip);
		end_op();
		return -1;
		// As mentioned above the the old path must be a path to a file
		// hardlinks cannot be created for directories
	}
	// ip declared in if((ip = namei(old)) == 0)
	ip->nlink++;
	// Incrementing number of hardlinks since we are adding a hardlink
	iupdate(ip);
	iunlock(ip);

	// Returns the name of the parrent directory and sets child as name
	// follow nameiparrent to see an example
	if((dp = nameiparent(new, name)) == 0)
		goto bad;
		// Could not resolve path aborting linking
	ilock(dp);
	// Checks that devices match in parrent and child inodes
	// Creates a dirent containg (name, inode_number) pair in the parent directory dp
	if(dp->dev != ip->dev || dirlink(dp, name, ip->inum) < 0){
		iunlockput(dp);
		goto bad;
		// Devs didnt match or dirent wasnt added succesfully
	}
	iunlockput(dp);
	iput(ip);

	end_op();

	return 0;
	// Success, official linked

bad:
	ilock(ip);
	ip->nlink--;
	iupdate(ip);
	iunlockput(ip);
	end_op();
	return -1;
	// Since the operation failed for some reason the link number that was incremented
	// above must be decremented
}

int
sys_symlink(void){
	char name[DIRSIZ], *new, *old;
	struct inode *dp, *ip, *new_node;

	if(argstr(0, &old) < 0 || argstr(1, &new) < 0)
		return -1;
	cprintf("old: %s\nnew: %s\n", old, new);

	begin_op();
	// Checks wether the node the user wants to link to exists
	if((ip = namei(old)) == 0){
		end_op();
		return -1;
		// Does not exist, cannot link to non existant file
	}

	ilock(ip);
	if(ip->type == T_DIR){
		iunlockput(ip);
		end_op();
		return -1;
		// implement symlinks to dirs later
	}
	// ip declared in if((ip = namei(old)) == 0)
	ip->nlink++;
	// Incrementing number of hardlinks since we are adding a hardlink
	iupdate(ip);
	iunlock(ip);
	
	if((dp = nameiparent(new, name)) == 0)
		goto bad;
		// Could not resolve path aborting linking
	ilock(dp);
	// Checks that devices match in parrent and child inodes
	// Creates a dirent containg (name, inode_number) pair in the parent directory dp
	//ip = create(path, T_SYMLINK, 0, 0)
	if ((new_node = create(new, T_SYMLINK, 0, 0)) == 0){
		goto bad;
	}
	// writei(dest, source, ofset, bytes)
	// now write the path to the inode with writei
	if(dp->dev != ip->dev || dirlink(dp, name, ip->inum) < 0){
		iunlockput(dp);
		goto bad;
		// Devs didnt match or dirent wasnt added succesfully
	}
	iunlockput(dp);
	iput(ip);

	end_op();

	return 0;
	// Success, official linked

bad:
	ilock(ip);
	ip->nlink--;
	iupdate(ip);
	iunlockput(ip);
	end_op();
	return -1;
	
}

// Is the directory dp empty except for "." and ".." ?
static int
isdirempty(struct inode *dp)
{
	int off;
	struct dirent de;
	// 2* sizeof(de), because . and .. are ignored
	for(off=2*sizeof(de); off<dp->size; off+=sizeof(de)){
		if(readi(dp, (char*)&de, off, sizeof(de)) != sizeof(de))
			panic("isdirempty: readi");
		if(de.inum != 0)
			return 0;
	}
	return 1;
}
// This function is called by rm userspace program, and removing/unliknig is essentially what it does
// This should work on the symlink you make
int
sys_unlink(void)
{
	struct inode *ip, *dp;
	struct dirent de;
	char name[DIRSIZ], *path;
	uint off;

	if(argstr(0, &path) < 0)
		return -1;

	begin_op();
	// sets the parrent dir to dp, and the name of the child to name
	// see nameiparent for an example
	if((dp = nameiparent(path, name)) == 0){
		end_op();
		return -1;
		// Path of the parrent directory does not exist error
	}

	ilock(dp);

	// Cannot unlink "." or "..".
	if(namecmp(name, ".") == 0 || namecmp(name, "..") == 0)
		goto bad;
		// these obviously exist in every dir and cannot be removed

	// looks for dirent name in directory dp
	// 0 is invalid inode, hence goto bad, otherwise
	// ip now points to the inode number associeated with name in dirent
	// and off is the offset at which this dirent was found
	if((ip = dirlookup(dp, name, &off)) == 0)
		goto bad;
		// dirlookup returned 0, failed
	ilock(ip);

	if(ip->nlink < 1)
		panic("unlink: nlink < 1");
	if(ip->type == T_DIR && !isdirempty(ip)){
		iunlockput(ip);
		goto bad;
		// directories cannot be deleted if they contain files
		// . and .. do not count
		// see isdirempty for concrete implementation
	}
	// sets the memory where de point to to all zeros, hence sizof(de)
	memset(&de, 0, sizeof(de));
	// writei writes the dirent de to the inode
	// since dirent de is all zeros, this effevtively overrides any past data
	if(writei(dp, (char*)&de, off, sizeof(de)) != sizeof(de))
		panic("unlink: writei");
		// write failed
	if(ip->type == T_DIR){
		dp->nlink--;
		iupdate(dp);
		// since the child ip of the parent dp was also a directory it influenced it's link cont
		// now that ip/ child is deleted the link count must be adjusted accordingly
	}
	iunlockput(dp);

	ip->nlink--;
	iupdate(ip);
	iunlockput(ip);

	end_op();

	return 0;

bad:
	iunlockput(dp);
	end_op();
	return -1;
}

static struct inode*
create(char *path, short type, short major, short minor)
{
	struct inode *ip, *dp;
	char name[DIRSIZ];
	// nameiparent -> namex, namex returns the parrent inode number and copies the 
	// final path element into name 
	// Parrent in this case meaning the inode that contained name as a dirent
	if((dp = nameiparent(path, name)) == 0)
		return 0;
		// ERROR no such path exists
	ilock(dp);
		// If name isn't empty after the above assigntmet to dp, it means thah the 
		// final element in the path was supposed to be a file
		// EXAMPLE /home/test/file if file isnt a directory the name wil be set to file,
		// otherwise the name wil be empty
	if((ip = dirlookup(dp, name, 0)) != 0){
		iunlockput(dp);
		ilock(ip);
		if((type == T_FILE && ip->type == T_FILE) || ip->type == T_DEV)
			return ip;
		// Basically if create tries to make an existing file, it returns the already existing file
		iunlockput(ip);
		return 0;
		//ERROR, if the name was still set after the nameiparetnt function call, 
		//it means that the original path ended with a file, 
		//and not a directory, and dp was assigned the
		// Inode of the parent directory of that file.
	}
	// create an inode for the file/dir, since the parrent directory inode is stored in the
	// dp pointer, the device dev can be extracted from it and the child will obviously
	// be on the same device as its parrent
	if((ip = ialloc(dp->dev, type)) == 0)
		panic("create: ialloc");

	ilock(ip);
	ip->major = major;
	ip->minor = minor;
	ip->nlink = 1;
	iupdate(ip);

	if(type == T_DIR){  // Create . and .. entries.
		dp->nlink++;  // for ".."
		iupdate(dp);
		// No ip->nlink++ for ".": avoid cyclic ref count.
		if(dirlink(ip, ".", ip->inum) < 0 || dirlink(ip, "..", dp->inum) < 0)
			// Dirlink returns 0 upon sucess otherwise 0
			panic("create dots");
		// ip is the inode pointer for the current directory and dp is the inode pointer
		// for the parrent directory, here by unix convention the "names" . and .. are
		// being linked to ip and dp respectively.

		// The above if implicitly makes two dirents in the current directory
	}

	if(dirlink(dp, name, ip->inum) < 0)
	// Be it a file or a directory the inode for the resulting aforementioned file/dir
	// has to be associated with a name, this part links it with that name
	// in the parrent diretory
		panic("create: dirlink");

	iunlockput(dp);

	return ip;
}

int
sys_open(void)
{
	char *path;
	int file_descriptor, omode;
	struct file *file;
	struct inode *inode_;
	// -1 is error code in xv6 hence <0
	if(argstr(0, &path) < 0 || argint(1, &omode) < 0)
		return -1;

	begin_op();
	// Omode can be the following
	// O_RDONLY  0x000
	// O_WRONLY  0x001
	// O_RDWR    0x002
	// O_CREATE  0x200
	// O_NOFOLLOW 0x003
	// Since ocreate has its bits in shifted to the left compared to the other O's this 
	// will ensure it only returns true in this conditional when anded with itsekf
	if(omode & O_CREATE){
		// Creates and returns an inode
		inode_ = create(path, T_FILE, 0, 0);
		if(inode_ == 0){
			//Inode 0 is the generic inode number to represent an error in creation
			// Therefore this returns -1
			end_op();
			return -1;
		}
	} else {
		if((inode_ = namei(path)) == 0){
			end_op();
			return -1;
			// In case this code is reached the inode in question is not a file but rather a dir or other
			// this if checks whether that dir or other exists, and retuns results accordingly
		}
		ilock(inode_);
		if(inode_->type == T_DIR && omode != O_RDONLY){
			iunlockput(inode_);
			end_op();
			return -1;
			// Cannot write to a directotry
			// at least not explicitly
			// dirents do this implicitly
		}
	}

	if((file = filealloc()) == 0 || (file_descriptor = fdalloc(file)) < 0){
		// The way this statement is set up the right side does not evaluate unless the 
		// the filealloc() retuns a non 0 result
		// if the right side is evaluated it means that the filealloc succseded and that
		// the file which was allocated on the left can be used as a variable
		if(file)
			fileclose(file);
		iunlockput(inode_);
		end_op();
		return -1;
		// Either filealloc returned 0 or fdalloc returned -1
		// file descriptor must be a positive value, also the first few natural numbers are reserved
		// for stdin and stdout
	}
	iunlock(inode_);
	end_op();

	file->type = FD_INODE;
	file->ip = inode_;
	file->off = 0;
	file->readable = !(omode & O_WRONLY);
	file->writable = (omode & O_WRONLY) || (omode & O_RDWR);
	return file_descriptor;
	// returns file descriptor which despite the name may refer to directories and many other things as well
	// The file descriptor links to an open file description, an entry in the system wide table of open files

	// for more information https://www.man7.org/linux/man-pages/man2/open.2.html#DESCRIPTION
}

int
sys_mkdir(void)
{
	char *path;
	struct inode *ip;

	begin_op();
	if(argstr(0, &path) < 0 || (ip = create(path, T_DIR, 0, 0)) == 0){
		end_op();
		return -1;
	}
	iunlockput(ip);
	end_op();
	return 0;
}

int
sys_mknod(void)
{
	struct inode *ip;
	char *path;
	int major, minor;

	begin_op();
	if((argstr(0, &path)) < 0 ||
			argint(1, &major) < 0 ||
			argint(2, &minor) < 0 ||
			(ip = create(path, T_DEV, major, minor)) == 0){
		end_op();
		return -1;
	}
	iunlockput(ip);
	end_op();
	return 0;
}

int
sys_chdir(void)
{
	char *path;
	struct inode *ip;
	struct proc *curproc = myproc();

	begin_op();
	if(argstr(0, &path) < 0 || (ip = namei(path)) == 0){
		end_op();
		return -1;
	}
	ilock(ip);
	if(ip->type != T_DIR){
		iunlockput(ip);
		end_op();
		return -1;
	}
	iunlock(ip);
	iput(curproc->cwd);
	end_op();
	curproc->cwd = ip;
	return 0;
}

int
sys_exec(void)
{
	char *path, *argv[MAXARG];
	int i;
	uint uargv, uarg;

	if(argstr(0, &path) < 0 || argint(1, (int*)&uargv) < 0){
		return -1;
	}
	memset(argv, 0, sizeof(argv));
	for(i=0;; i++){
		if(i >= NELEM(argv))
			return -1;
		if(fetchint(uargv+4*i, (int*)&uarg) < 0)
			return -1;
		if(uarg == 0){
			argv[i] = 0;
			break;
		}
		if(fetchstr(uarg, &argv[i]) < 0)
			return -1;
	}
	return exec(path, argv);
}

int
sys_pipe(void)
{
	int *fd;
	struct file *rf, *wf;
	int fd0, fd1;

	if(argptr(0, (void*)&fd, 2*sizeof(fd[0])) < 0)
		return -1;
	if(pipealloc(&rf, &wf) < 0)
		return -1;
	fd0 = -1;
	if((fd0 = fdalloc(rf)) < 0 || (fd1 = fdalloc(wf)) < 0){
		if(fd0 >= 0)
			myproc()->ofile[fd0] = 0;
		fileclose(rf);
		fileclose(wf);
		return -1;
	}
	fd[0] = fd0;
	fd[1] = fd1;
	return 0;
}

int sys_getcwd(void){
	
}