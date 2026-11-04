/** @brief adc (derived from Xilinx DebugDev)
 * 
 */


/* address space definition (offsets within target) */
#define ADC_CSR_REGISTER 0x0
#define ADC_COMPUTE 0x04

#include <inttypes.h>

using namespace sc_core;
using namespace std;

class adc
	: public sc_core::sc_module
{
	uint32_t CSR{};
	sc_core::sc_event ADCInvoked;

public:
	tlm_utils::simple_target_socket<adc> socket;
	sc_out<bool> irq;

	adc(sc_core::sc_module_name name);
	virtual void b_transport(tlm::tlm_generic_payload &trans,
							 sc_time &delay);
	virtual unsigned int transport_dbg(tlm::tlm_generic_payload &trans);

	void op_thread();
};
