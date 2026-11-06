#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/mman.h>
#include <fcntl.h>

/// physical base addresses of the SystemC devices
#define VECPROC_ADDR  (0x49000000)
#define RECEIVER_ADDR (0x50000000)

/// register offsets
#define CSR        0x00
#define VA_BASE    0x04
#define VB_BASE    0x44
#define VC_BASE    0x84
#define VEC_WORDS  16
#define MAC_OPCODE 5

/// map one device's page of physical memory into this process
static void *map_device(int fd, unsigned long phys, unsigned page_size)
{
	void *p = mmap(NULL, page_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd,
		       phys & ~(page_size - 1));
	if (p == MAP_FAILED) {
		perror("mmap");
		exit(-1);
	}
	return p;
}

int main(int argc, char *argv[])
{
	// Matched-filter weights for the Q tone, interleaved [I, Q] like VB
	const int32_t weights[VEC_WORDS] = { 0, 0,  0, 1,  0, 0,  0, -1, 0, 0,  0, 1,  0, 0,  0, -1 };
	
	unsigned page_size = sysconf(_SC_PAGESIZE);
	unsigned int pollingCounts = 0;
	int fd;

	(void)argc;

	fd = open("/dev/mem", O_RDWR | O_SYNC);
	if (fd < 1) {
		perror(argv[0]);
		exit(-1);
	}

	void *vec = map_device(fd, VECPROC_ADDR, page_size);
	void *rx  = map_device(fd, RECEIVER_ADDR, page_size);

	// Initialize the Vector Processor's Registers
		// Load the weights into VA
	for (int i = 0; i < VEC_WORDS; i++)
		*(volatile unsigned int*)(vec + VA_BASE + 4 * i) = weights[i];

	// Clear VB (the RX Data accumulates into it)
	// Clear VC (the MAC accumulates into it)
	for (int i = 0; i < VEC_WORDS; i++)
	{
		*(volatile unsigned int*)(vec + VB_BASE + 4 * i) = 0;
		*(volatile unsigned int*)(vec + VC_BASE + 4 * i) = 0;
	}

	// Power On the Rx: Rx -> XADC -> VecProc runs in hardware from here
	*(volatile unsigned int*)(rx + CSR) = 1;
	printf("CPU : Receiver powered on\n");

	// Poll the Vector Processor CSR: 0 (idle) -> 5 (MAC running) -> 0 (done)
	while (*(volatile unsigned int*)(vec + CSR) == 0)
		pollingCounts++;

	while (*(volatile unsigned int*)(vec + CSR) == MAC_OPCODE)
		pollingCounts++;

	printf("CPU : MAC done, pollingCounts = %u\n", pollingCounts);

	// Read VC as [I, Q] pairs, one sample per line
	for (int i = 0; i < VEC_WORDS; i += 2) {
		int32_t i_val = *(volatile int*)(vec + VC_BASE + 4 * i);
		int32_t q_val = *(volatile int*)(vec + VC_BASE + 4 * (i + 1));
		printf("CPU VC[%2d], VC[%2d] : %d, %d\n", i, i + 1, i_val, q_val);
	}

	// TearDown Code
	munmap(vec, page_size);
	munmap(rx, page_size);
	close(fd);
	
	return 0;
}
