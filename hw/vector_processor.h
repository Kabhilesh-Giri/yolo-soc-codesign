/** @brief Vector Processor (derived from Xilinx DebugDev)
 * 
 */


/* address space definition (offsets within target) */
#define CSR_REGISTER 0x0
#define VA_BASE 0x04     // VA The base address of the first vector operand
#define VB_BASE 0x44     // VB The base address of the second vector operand
#define VC_BASE 0x84     // VC The base address of the output vector
#define MMR_TRACE 0xC4

#define VEC_WORDS 16 			  // 16 Word Count
#define VEC_BYTES (VEC_WORDS * 4) // uint32_t -> 32 bits or 4 Bytes

#include <inttypes.h>

class vector_processor
	: public sc_core::sc_module
{
	uint32_t CSR{};
	sc_core::sc_event vectorAddStart;
	sc_core::sc_event vectorSubStart;
	sc_core::sc_event vectorMulStart;
	sc_core::sc_event vectorDivStart;
	sc_core::sc_event vectorMACStart;

	uint32_t VA[VEC_WORDS];
	uint32_t VB[VEC_WORDS];
	uint32_t VC[VEC_WORDS];

	uint32_t index = 0;
	uint32_t bytes = 0;
public:
	tlm_utils::simple_target_socket<vector_processor> socket;
	sc_out<bool> irq;

	vector_processor(sc_core::sc_module_name name);
	virtual void b_transport(tlm::tlm_generic_payload &trans,
							 sc_time &delay);
	virtual unsigned int transport_dbg(tlm::tlm_generic_payload &trans);

	void op_thread();

	void vectorADD();
	void vectorSub();
	void vectorMul();
	void vectorDiv();
	void vectorMAC();
};
