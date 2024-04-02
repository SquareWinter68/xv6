#ifndef FILE_H
#define FILE_H

struct file {
	enum { FD_NONE, FD_PIPE, FD_INODE } type;
	int ref; // reference count
	char readable;
	char writable;
	struct pipe *pipe;
	struct inode *ip;
	uint off;
};
// ==================================IMPORTANT====================================================
// The inode structute below is not the inode stored on the disk, it contains the same
// data + some other helpful stuff, but this one is stored in memory
// ==================================IMPORTNT=====================================================

// in-memory copy of an inode
struct inode {
	uint dev;           // Device number
	uint inum;          // Inode number
	int ref;            // Reference count
	struct sleeplock lock; // protects everything below here
	int valid;          // inode has been read from disk?

	short type;         // copy of disk inode
	short major;		// This denotes the general class of device
	short minor;		// This denotes the specific instanace see below
	short nlink;		// number of hard links to the file
	uint size;			// Number of bytes for the file, in case of symlinks only the path length
						// more exaustive info can be found in the link below
	uint addrs[NDIRECT+1];	// adress for the file data
};

// table mapping major device number to
// device functions
struct devsw {
	// unassigned function pointers
	int (*read)(struct inode*, char*, int);
	int (*write)(struct inode*, char*, int);
};

extern struct devsw devsw[];

#define CONSOLE 1

#endif

// https://www.man7.org/linux/man-pages/man7/inode.7.html#DESCRIPTION

//  Device where inode resides
//               stat.st_dev; statx.stx_dev_minor and statx.stx_dev_major
//               Each inode (as well as the associated file) resides in a
//               filesystem that is hosted on a device.  That device is
//               identified by the combination of its major ID (which
//               identifies the general class of device) and minor ID
//               (which identifies a specific instance in the general
//               class).
