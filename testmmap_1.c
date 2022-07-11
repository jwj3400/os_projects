#include "types.h"
#include "user.h"
#include "param.h"
#include "fcntl.h"

int main(int argc, char** argv) {
	int i;
	int size = 8192;
	int fd = open("README", O_RDWR);
	char* text = (char*)mmap(0, size, PROT_READ | PROT_WRITE, MAP_SHARED|MAP_POPULATE, fd, 0);	//File example
//	char* text2 = (char*)mmap(0, size, PROT_WRITE|PROT_READ, MAP_POPULATE|MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);  //ANONYMOUS example
//	char* text = (char*)mmap(4096, size, PROT_WRITE|PROT_READ, MAP_PRIVATE|MAP_POPULATE|MAP_FIXED, fd, 0); // FIXED example
	text[0] = 'k';
	for (i = 0; i < size; i++) 
		printf(1, "%c", text[i]);
	printf(1,"\n============file mmap end==========\n\n\n\n");

/*	text2[0] = 's';					//40002000
	text2[4096] = 'Y';				//40003000
	for (i = 0; i < size; i++) 
		printf(1, "%c", text2[i]);
	printf(1,"\n============anonymous mmap end==========\n");
*/
	munmap((uint)text);
//	munmap((uint)text2);

	exit();
}
