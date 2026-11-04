/** @brief Receiver (derived from Xilinx DebugDev)
 * 
 */


/* address space definition (offsets within target) */
#define CSR_REGISTER 0x0

#include <inttypes.h>

#include <fstream>
#include <sstream>
#include <string>

#include "tlm_utils/simple_target_socket.h"
#include "tlm_utils/simple_initiator_socket.h"

using namespace sc_core;
using namespace std;

class receiver
	: public sc_core::sc_module
{
	uint32_t CSR{};
	int32_t TLMPacket{};
	sc_core::sc_event receiverPowerOn;

public:
	tlm_utils::simple_target_socket<receiver> socket;
	tlm_utils::simple_initiator_socket<receiver> initiator_socket;
	sc_out<bool> irq;

	tlm::tlm_generic_payload *trans;
	unsigned long long baseAddr = 0x60000000; // XADC Device Address

	receiver(sc_core::sc_module_name name);
	virtual void b_transport(tlm::tlm_generic_payload &trans,
							 sc_time &delay);
	virtual unsigned int transport_dbg(tlm::tlm_generic_payload &trans);

	void op_thread();
	void read_csv();

	void resetCSRBit();
	void setCSRBit();
	
	int32_t bitPacking(string, string, string);
	void sendTLMDataToXADC(int32_t);
};
