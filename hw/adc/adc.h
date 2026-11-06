/** @brief adc (derived from Xilinx DebugDev)
 * 
 */


/* address space definition (offsets within target) */
#include <tlm_utils/simple_initiator_socket.h>
#define ADC_CSR_REGISTER 0x0
#define ADC_COMPUTE 0x04
#define VB_BASE 0x44 // XADC will populate the Vector B Spacings

#include <inttypes.h>
#include <cmath>
#include <algorithm>

using namespace sc_core;
using namespace std;

class adc
	: public sc_core::sc_module
{
	uint32_t CSR{};
	uint32_t packedData{};

	int32_t counter {};
	uint32_t index {0x00};
	double frontEndScaling {0.5}; // Gain or Attenuation
	double LSB = {1.0 / 4096}; // LSB = 0.244 milli Volt

	sc_fifo<uint32_t> fifo;

	// Intermediate Exchange Values 
	uint16_t time_value{};
	int8_t i_value{}, q_value{};
	double i_scaledValue {}, q_scaledValue{};
	int16_t i_code{}, q_code{};

	bool busy {}; // op_thread is processing a sample

public:
	tlm_utils::simple_target_socket<adc> socket;
	tlm_utils::simple_initiator_socket<adc> initiator_socket;

	sc_out<bool> irq;

	adc(sc_core::sc_module_name name);

	tlm::tlm_generic_payload *trans;

	virtual void b_transport(tlm::tlm_generic_payload &trans,
							 sc_time &delay);
	virtual unsigned int transport_dbg(tlm::tlm_generic_payload &trans);

	void op_thread();

	void bitUnPacking();

	void analogFrontEnd();

	void XADC_TwoTwelveBits();

	void sendTLMDataToVecProc(uint32_t, uint32_t);

	void TriggerMAC();
};
