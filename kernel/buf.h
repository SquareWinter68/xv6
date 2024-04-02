#ifndef BUFFER_H
#define BUFFER_H

struct buf {
	int flags;
	uint dev;
	uint blockno;
	struct sleeplock lock;
	uint refcnt;
	struct buf *prev; // LRU cache list
	struct buf *next;
	struct buf *qnext; // disk queue
	uchar data[BSIZE];
};
#define B_VALID 0x2  // buffer has been read from disk
#define B_DIRTY 0x4  // buffer needs to be written to disk

#endif
// Lru stands for least recently used
// these blocks are indeed written to the disk, but when read from the disk they are kept in the 
// LRU cache in case they are needed later so thah they would not have to be fetched from the disk
// because this is a time consumng operation
// This cache seems to behave like a linked list, and the least recently used elements are likely
// as the name implies to be deleted firs