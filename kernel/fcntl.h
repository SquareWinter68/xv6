#ifndef FCNTL_H
#define FCNTL_H

#define O_RDONLY  0x000
#define O_WRONLY  0x001
#define O_RDWR    0x002
#define O_CREATE  0x200
#define O_NOFOLLOW 0x004
// O_nofollow does not collide with any other flags probably
//something that would give true only when anded with itself

#endif