/*
 * adc Device 
 */

#define SC_INCLUDE_DYNAMIC_PROCESSES

#include <inttypes.h>

#include <fstream>
#include <sstream>
#include <string>

#include "tlm_utils/simple_target_socket.h"
#include "adc.h"

adc::adc(sc_module_name name)
	: sc_module(name), socket("socket")
{
	socket.register_b_transport(this, &adc::b_transport);
	socket.register_transport_dbg(this, &adc::transport_dbg);

	SC_THREAD(op_thread);
}

void adc::op_thread()
{
	for(;;)
	{	
		wait(ADCInvoked);
		printf("ADC Started \n");
		wait(5, SC_MS); // Wait for 5 ms when it gets notified
		CSR = 0; // Operation has concluded reset the CSR Bit
	}
}

// called when a TLM transaction arrives for this target
void adc::b_transport(tlm::tlm_generic_payload &trans, sc_time &delay)
{
	tlm::tlm_command cmd = trans.get_command();
	sc_dt::uint64 addr = trans.get_address();
	unsigned char *data = trans.get_data_ptr();
	unsigned int len = trans.get_data_length();
	unsigned char *byt = trans.get_byte_enable_ptr();
	unsigned int wid = trans.get_streaming_width();

	// transactions with separate byte lanes are not supported
	if (byt != 0) {
		trans.set_response_status(tlm::TLM_BYTE_ENABLE_ERROR_RESPONSE);
		return;
	}

	// bursts not supported
	if (len > 4 || wid < len) {
		trans.set_response_status(tlm::TLM_BURST_ERROR_RESPONSE);
		return;
	}

	trans.set_response_status(tlm::TLM_OK_RESPONSE);

	delay += sc_time(1, SC_US);
	wait(delay);
	delay = sc_time(0, SC_US);

	// compute current time (incl. any quantum offset if no sync above)
	sc_time now = sc_time_stamp() + delay;

	// handle reads commands
	if (cmd == tlm::TLM_READ_COMMAND) {
		static sc_time old_ts = SC_ZERO_TIME, diff;
		uint32_t v = 0;
		switch (addr)
		{
		case ADC_CSR_REGISTER:
			v = CSR;
			break;

		case ADC_COMPUTE:
			if(CSR == 1)
				ADCInvoked.notify();
			break;
			
		default:
			break;
		}
		memcpy(data, &v, len);

	// handle write commands
	} else if (cmd == tlm::TLM_WRITE_COMMAND) {
		static sc_time old_ts = SC_ZERO_TIME, diff;
		printf("Got the call\n");
		switch (addr) {	

		case ADC_CSR_REGISTER:
			printf("Got the call 1\n");
			CSR = *(unsigned int *)data; // Set or Reset
            break;

		default:
			printf("Got the call 2\n");
			break;
		}
	} else {
		// no other commands supported
		trans.set_response_status(tlm::TLM_COMMAND_ERROR_RESPONSE);
	}
}

unsigned int adc::transport_dbg(tlm::tlm_generic_payload &trans)
{
	unsigned int len = trans.get_data_length();
	return len;
}
