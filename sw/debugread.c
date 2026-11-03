#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>

/// base address of debug device (physical)
#define SYSTEMC_DEVICE_ADDR (0x48000000)

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
	
	printf("Reading debugdev timer every 200ms\n");
	for(int i = 0; i < 10; i++) {
		usleep(200000);
		// read the base register of the debug device (offset 0)
		unsigned int val = *((volatile unsigned int*) (pDev+ 0));
		printf("TIMER = %u\n", val);
	}

	char data[] = "Cosimulation Works\n";
	
	unsigned int i = 0;
	while(data[i] != '\n')
	{
		*(volatile char*)(pDev+ 0x4) = data[i];
		i++;
	}
	*(volatile char*)(pDev+ 0x4) = data[i];

	// reading from 0xf0
	unsigned char val1 = *((volatile unsigned char*) (pDev+ 0xf0));
	printf("Reading from 0xf0 = %u\n", val1);

	// writing into calling 0x8 case to stop the simulation
	unsigned int hexdata = 32;
	*(unsigned int*)(pDev + 0x8) = hexdata;

	printf("Print Statement to check whether the simulation is stopped or not\n");

	return 0; 
}