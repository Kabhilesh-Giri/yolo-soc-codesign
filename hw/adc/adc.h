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
	uint32_t packedData{};

	double frontEndScaling {0.5}; // Gain or Attenuation
	double LSB = {1.0 / 4096}; // LSB = 0.244 milli Volt

	// Intermediate Exchange Values 
	uint16_t time_value{};
	int8_t i_value{}, q_value{};
	double i_scaledValue {}, q_scaledValue{};
	int32_t i_code{}, q_code{};

	sc_core::sc_event ADCInvoked;

public:
	tlm_utils::simple_target_socket<adc> socket;
	sc_out<bool> irq;

	adc(sc_core::sc_module_name name);
	virtual void b_transport(tlm::tlm_generic_payload &trans,
							 sc_time &delay);
	virtual unsigned int transport_dbg(tlm::tlm_generic_payload &trans);

	void op_thread();

	void bitUnPacking(uint32_t, uint16_t&, int8_t&, int8_t&);

	void analogFrontEnd(const int8_t&, const int8_t&, double&, double&);
};
