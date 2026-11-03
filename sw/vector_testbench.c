#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/time.h>
#include <time.h>

/// base address of debug device (physical)
#define SYSTEMC_DEVICE_ADDR (0x49000000)
#define VEC_WORDS 16

void writeVector(void *pDev, unsigned int base, unsigned int endAddr, unsigned int sizeofBytes, unsigned int values[])
{
	for(unsigned int i = base; i < endAddr; i += sizeofBytes)
	{
		unsigned int value = ((unsigned int)rand() << 16) ^ (unsigned int)rand();
		values[(i - base) / sizeofBytes] = value; // remember it for the reference
		*(volatile unsigned int*) (pDev + i) = value; 
	}
}

void readVector(void *pDev, unsigned int base, unsigned int endAddr, unsigned int sizeofBytes, unsigned int values[])
{
	for(unsigned int i = base; i < endAddr; i += sizeofBytes)
	{
		unsigned int r1 = *(volatile unsigned int*) (pDev + i);
		values[(i - base) / sizeofBytes] = r1; // remember it for the reference
	}
}

unsigned int compareVector(void *pDev, unsigned int base, unsigned int endAddr, unsigned int sizeofBytes, unsigned int expected[])
{
	unsigned int errors = 0;
	for(unsigned int i = base; i < endAddr; i += sizeofBytes)
	{
		unsigned int r1 = *(volatile unsigned int*) (pDev + i);
		unsigned int index = (i - base) / sizeofBytes;
		if(r1 != expected[index])
		{
			printf("Mismatch at Vector Index %u : got %u, expected %u\n", index, r1, expected[index]);
			errors++;
		}
	}
	return errors;
}

void execute(void *pDev, unsigned int csr_bit, unsigned int sizeofBytes, unsigned int refC[VEC_WORDS], unsigned int operation)
{
	int pollingCounts = 0;

	// Initialize QEMU Timer
	struct timeval current_time_start;
	struct timeval current_time_stop;

	// Start the QEMU Timer 
	gettimeofday(&current_time_start, NULL);

	// Start the SystemC Timer Using 0xC4 
	unsigned int systemCTimer_start = *(volatile unsigned int*)(pDev + 0xC4);

	// Set the CSR bit According to the Operation
	*(volatile unsigned int*)(pDev + 0x0) = operation;
	csr_bit = *(volatile unsigned int*)(pDev + 0x0);
	printf("CSR Bit set to trigger the vector processor : %u\n", csr_bit);

	// ----------------- Polling -----------------
	while(csr_bit == operation)
	{
		// Read the CSR bit
		csr_bit = *(volatile unsigned int*)(pDev + 0x0);
		pollingCounts++;
	}

	// Stop the QEMU & SystemC Timer
	gettimeofday(&current_time_stop, NULL);
	unsigned int systemCTimer_stop = *(volatile unsigned int*)(pDev + 0xC4);

	// ----------------- Output ------------------

	// Read Vector C
	unsigned int baseC = 0x84;
	unsigned int endAddrC = 0xC3;

	// Compare Vector C with the reference
	unsigned int errors = compareVector(pDev, baseC, endAddrC, sizeofBytes, refC);
	printf("Vector %s : %u mismatches\n", errors == 0 ? "PASS" : "FAIL", errors);

	printf("CSR Bit After Vector Processor Complete : %u\n", csr_bit);
	printf("Polling Counts : %u\n", pollingCounts);

	// Convert QEMU timeval structs to total nanoseconds (using 64-bit integers to prevent overflow)
    int64_t qemu_start_ns = (int64_t)current_time_start.tv_sec * 1000000000LL + (int64_t)current_time_start.tv_usec * 1000LL;
    int64_t qemu_stop_ns  = (int64_t)current_time_stop.tv_sec * 1000000000LL + (int64_t)current_time_stop.tv_usec * 1000LL;

	printf("----------- QEMU Time -----------\n");
	printf("QEMU Time Consumption in nanoseconds : %lld\n", (long long)(qemu_stop_ns - qemu_start_ns));

	printf("----------- SystemC Time -----------\n");
	printf("SystemC Time Consumption in nanoseconds : %u\n", (systemCTimer_stop - systemCTimer_start));
}

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

	// Word Length 
	unsigned int sizeofBytes = 4;
	enum Operation {
		ADD = 1,
		SUB = 2,
		MUL = 3,
		DIV = 4,
		MAC = 5
	};	
	
	// New random values on every run
	srand(time(NULL));

	unsigned int vecA[VEC_WORDS] = {0}, vecB[VEC_WORDS] = {0}, refC[VEC_WORDS] = {0};

	// Write Vector A
	unsigned int baseA = 0x04;
	unsigned int endAddrA = 0x43;
	writeVector(pDev, baseA, endAddrA, sizeofBytes, vecA);

	// Write Vector B
	unsigned int baseB = 0x44;
	unsigned int endAddrB = 0x83;
	writeVector(pDev, baseB, endAddrB, sizeofBytes, vecB);

	// Read the CSR bit
	unsigned int csr_bit = *(volatile unsigned int*)(pDev + 0x0);
	printf("CSR Bit before setting up : %u\n", csr_bit);
	
	printf("----------- Vector ADD Operation -----------\n");

	unsigned int operation = ADD;	

	// Reference output Vector ADD Operation computed in userspace 
	for(unsigned int i = 0; i < VEC_WORDS; i++)
	{
		refC[i] = vecA[i] + vecB[i];
	}
	
	execute(pDev, csr_bit, sizeofBytes, refC, operation);

		
	printf("----------- Vector SUB Operation -----------\n");

	operation = SUB;
	// Reference output Vector SUB Operation computed in userspace 
	for(unsigned int i = 0; i < VEC_WORDS; i++)
	{
		refC[i] = vecA[i] - vecB[i];
	}

	execute(pDev, csr_bit, sizeofBytes, refC, operation);

	printf("----------- Vector MUL Operation -----------\n");

	operation = MUL;

	// Reference output Vector MUL Operation computed in userspace 
	for(unsigned int i = 0; i < VEC_WORDS; i++)
	{
		refC[i] = vecA[i] * vecB[i];
	}

	execute(pDev, csr_bit, sizeofBytes, refC, operation);

	printf("----------- Vector DIV Operation -----------\n");

	operation = DIV;

	// Reference output Vector DIV Operation computed in userspace 
	for(unsigned int i = 0; i < VEC_WORDS; i++)
	{
		if(vecB[i] != 0)
			refC[i] = vecA[i] / vecB[i];
		else
			refC[i] = 0;
	}

	execute(pDev, csr_bit, sizeofBytes, refC, operation);
	
	printf("----------- Vector MAC Operation -----------\n");

	operation = MAC;
	
	// Read Vector C
	unsigned int baseC = 0x84;
	unsigned int endAddrC = 0xC3;
	readVector(pDev, baseC, endAddrC, sizeofBytes, refC);

	// Reference output Vector DIV Operation computed in userspace 
	for(unsigned int i = 0; i < VEC_WORDS; i++)
	{
		refC[i] += (vecA[i] * vecB[i]);
	}

	execute(pDev, csr_bit, sizeofBytes, refC, operation);

	return 0; 
}
