// lz4 trivial example : print library version number
// copyright : takayuki matsuoka & yann collet


#include <stdio.h>
#include "lz4.h"

int main(int argc, char** argv)
{
	(void)argc; (void)argv;
    printf("hello world ! lz4 library version = %d\n", lz4_versionnumber());
    return 0;
}
