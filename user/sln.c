#include "kernel/types.h"
#include "kernel/stat.h"
#include "user.h"

int
main(int argc, char *argv[])
{
	if(argc != 3){
		fprintf(2, "Usage: ln old new\n");
		exit();
		// make sure there are only three arguments.
		// well actually two but it counts the sln as well
	}
	if(symlink(argv[1], argv[2]) < 0)
		fprintf(2, "link %s %s: failed\n", argv[1], argv[2]);
	exit();
}
