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
	
	trans = new tlm::tlm_generic_payload;

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

void adc::sendTLMDataToVecProc(uint32_t data, uint32_t index)
{
	unsigned long long addr = 0x49000000 + VB_BASE + index;

	trans->set_command(tlm::TLM_WRITE_COMMAND);
	trans->set_address(addr);
	trans->set_data_ptr(reinterpret_cast<unsigned char *>(&data));
	trans->set_data_length(4);
	trans->set_streaming_width(4);                            // = data_length to indicate no streaming
	trans->set_byte_enable_ptr(0);                            // 0 indicates unused
	trans->set_dmi_allowed(false);                            // Mandatory initial value
	trans->set_response_status(tlm::TLM_INCOMPLETE_RESPONSE); // Mandatory initial value

	sc_time delay(SC_ZERO_TIME);
	initiator_socket->b_transport(*trans, delay);
	
	// Initiator obliged to check response status
	if (trans->is_response_error())
		SC_REPORT_ERROR("TLM-2", trans->get_response_string().c_str());
	else
		printf("Data Transfered from XADC to Vector Processor : %d\n", data);
}

void adc::TriggerMAC()
{
	// Executer the MAC 
	unsigned long long addr = 0x49000000; // #define CSR_REGISTER 0x0

	unsigned int data = 5; // CSR MAC Address
	trans->set_command(tlm::TLM_WRITE_COMMAND);
	trans->set_address(addr);
	trans->set_data_ptr(reinterpret_cast<unsigned char *>(&data));
	trans->set_data_length(4);
	trans->set_streaming_width(4);                            // = data_length to indicate no streaming
	trans->set_byte_enable_ptr(0);                            // 0 indicates unused
	trans->set_dmi_allowed(false);                            // Mandatory initial value
	trans->set_response_status(tlm::TLM_INCOMPLETE_RESPONSE); // Mandatory initial value

	sc_time delay(SC_ZERO_TIME);
	initiator_socket->b_transport(*trans, delay);
	
	// Initiator obliged to check response status
	if (trans->is_response_error())
		SC_REPORT_ERROR("TLM-2", trans->get_response_string().c_str());
	else
		printf("TriggerMAC Transfered from XADC to Vector Processor : %d\n", data);

}

void adc::op_thread()
{
	constexpr uint32_t VB_BYTES = 16 * 4;
	for(;;)
	{	
		packedData = fifo.read(); // blocks until the RX has sent the packet 
		busy = true;

		bitUnPacking();

		analogFrontEnd(); // Rescaled from +-1 V to +- 0.5 V
		XADC_TwoTwelveBits(); // Divide -> Round -> Clamp -> Store

		// VB holds 16 words : never write past it into VC

		if(index + 8 <= VB_BYTES)
		{
			sendTLMDataToVecProc(i_code, index); 
			index += 0x04;
			sendTLMDataToVecProc(q_code, index);
			index += 0x04;
			counter++;
		}
		else
		{
			printf("XADC : VB full, sample dropped\n");
		}

		busy = false;

		if(CSR == 0 && fifo.num_available() == 0)
			printf("Total Conversion : %d \n", counter);

		if (index == VB_BYTES)
			TriggerMAC();
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
			if(CSR == 1)
			{
				// New run : fill VB from the start 
				index = 0;
				counter = 0;
			}
			else if(CSR == 0 && !busy && fifo.num_available() == 0)
				printf("Total Conversion : %d \n", counter);
			break;

		case ADC_COMPUTE:
			if(CSR == 1)
			{
				fifo.write(*(unsigned int *)data);
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
