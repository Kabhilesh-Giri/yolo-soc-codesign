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

void adc::bitUnPacking(uint32_t packedData, uint16_t& time_value, 
	int8_t& i_value, int8_t& q_value)
{
	time_value = (packedData >> 16) & 0xFFFFu;
	i_value = static_cast<int8_t>((packedData >> 8) & 0xFFu);
	q_value = static_cast<int8_t>(packedData & 0xFFu);
}

void adc::analogFrontEnd(const int8_t& i_value,const int8_t& q_value, double& i_scaledValue, double& q_scaledValue)
{
	i_scaledValue = i_value * frontEndScaling;	
	q_scaledValue = q_value * frontEndScaling; // front end gain = 0.5
}

void adc::op_thread()
{
	for(;;)
	{	
		wait(ADCInvoked);

		bitUnPacking(packedData, time_value, i_value, q_value);
		
		analogFrontEnd(i_value, q_value, i_scaledValue, q_scaledValue); // Rescaled from +-1 V to +- 0.5 V

		i_code = i_scaledValue / LSB; // Find the I & Q Codes
		q_code = q_scaledValue / LSB;

		printf("packedData : %u | %08X\n", packedData, packedData);
		printf("UnPackedData time_ns = %08X, i = %f, q = %f\n", time_value, i_scaledValue, q_scaledValue);
		printf("time_ns = %08X, i = %d, q = %d\n", time_value, i_code, q_code);
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
			// Placehold
			break;
			
		default:
			break;
		}
		memcpy(data, &v, len);

	// handle write commands
	} else if (cmd == tlm::TLM_WRITE_COMMAND) {
		static sc_time old_ts = SC_ZERO_TIME, diff;
		switch (addr) {	

		case ADC_CSR_REGISTER:
			CSR = *(unsigned int *)data; // Set or Reset
			break;

		case ADC_COMPUTE:
			if(CSR == 1)
			{
				packedData = *(unsigned int *)data;
				ADCInvoked.notify();
			}
            break;

		default:
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
