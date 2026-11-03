/** @brief Receiver (derived from Xilinx DebugDev)
 * 
 */


/* address space definition (offsets within target) */
#define CSR_REGISTER 0x0
#define PATH_ADC 0x01     // VA The base address of the first vector operand
#define PATH_APU 0x02     // VB The base address of the second vector operand
#define MMR_TRACE 0x03

#include <inttypes.h>

class receiver
	: public sc_core::sc_module
{
	uint32_t CSR{};
	sc_core::sc_event receiverAPUPath;
	sc_core::sc_event receiverADCPath;

public:
	tlm_utils::simple_target_socket<receiver> socket;
	sc_out<bool> irq;

	receiver(sc_core::sc_module_name name);
	virtual void b_transport(tlm::tlm_generic_payload &trans,
							 sc_time &delay);
	virtual unsigned int transport_dbg(tlm::tlm_generic_payload &trans);

	void op_thread();
};
