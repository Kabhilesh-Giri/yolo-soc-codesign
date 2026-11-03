#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>

/// base address of debug device (physical)
#define SYSTEMC_DEVICE_ADDR (0x50000000)

int main(int argc, char *argv[])
{
	int fd;       /// file descriptor to phys mem
	void *pDev;   /// pointer to base address of device (mapped into user's virtual mem)
	unsigned page_size=sysconf(_SC_PAGESIZE);  /// get page size 

	//open device file representing physical memory 
	fd=open("/dev/mem",O_RDWR);
	if(fd<1) {
		perror(argv[0]);
		exit(-1);
	}


	/// get a pointer in process' virtual memory that points to the physcial address of the device
	pDev=mmap(NULL,page_size,PROT_READ|PROT_WRITE,MAP_SHARED,fd,(SYSTEMC_DEVICE_ADDR & ~(page_size-1)));
	
	return 0; 
}