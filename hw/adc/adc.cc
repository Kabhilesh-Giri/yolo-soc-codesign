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

void adc::bitUnPacking()
{
	time_value = (packedData >> 16) & 0xFFFFu;
	i_value = static_cast<int8_t>((packedData >> 8) & 0xFFu);
	q_value = static_cast<int8_t>(packedData & 0xFFu);
}

void adc::analogFrontEnd()
{
	i_scaledValue = i_value * frontEndScaling;	
	q_scaledValue = q_value * frontEndScaling; // front end gain = 0.5

	if(i_scaledValue > 2047)
		i_scaledValue = 2047;

	if(q_scaledValue > 2047)
		q_scaledValue = 2047;

	if(i_scaledValue < -2048)
		i_scaledValue = -2048;
		
	if(q_scaledValue > -2048)
		q_scaledValue = -2048;
}

void adc::XADC_TwoTwelveBits()
{
	// 12 bit signed (bipolar) code range
	constexpr long CODE_MIN = -2048;
	constexpr long CODE_MAX = 2047;

	// Divide -> Round 
	long i_raw = std::lround(i_scaledValue / LSB);
	long q_raw = std::lround(q_scaledValue / LSB);

	// Clamp (saturate , never wrap)
	i_raw = std::clamp(i_raw, CODE_MIN, CODE_MAX);
	q_raw = std::clamp(q_raw, CODE_MIN, CODE_MAX);

	// Store
	i_code = static_cast<int16_t>(i_raw);
	q_code = static_cast<int16_t>(q_raw);
}

void adc::op_thread()
{
	for(;;)
	{	
		wait(ADCInvoked);

		bitUnPacking();
		
		analogFrontEnd(); // Rescaled from +-1 V to +- 0.5 V

		XADC_TwoTwelveBits(); // Divide -> Round -> Clamp -> Store

		printf("packedData : %u | %08X\n", packedData, packedData);
		printf("UnPackedData time_ns = %d, i = %f, q = %f\n", time_value, i_scaledValue, q_scaledValue);
		printf("time_ns = %d, i = %d, q = %d, i = %X, q = %X\n", time_value, i_code, q_code, i_code, q_code);
		counter++;
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
