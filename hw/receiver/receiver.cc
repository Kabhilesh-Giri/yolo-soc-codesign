/*
 * Receiver Device 
 */

#define SC_INCLUDE_DYNAMIC_PROCESSES
#include "receiver.h"

receiver::receiver(sc_module_name name)
	: sc_module(name), socket("socket")
{
	socket.register_b_transport(this, &receiver::b_transport);
	socket.register_transport_dbg(this, &receiver::transport_dbg);
	trans = new tlm::tlm_generic_payload;
	SC_THREAD(op_thread);
}

int32_t receiver::bitPacking(string time_ns, string i, string q)
{
	//Convert String -> 16 bit time value | 8 bit i value | 8 bit q value & Mask
	int32_t time_value = static_cast<int32_t>(std::stoi(time_ns)) & 0xFFFF;
	int16_t i_value = static_cast<int32_t>(std::stoi(i))		  &	0xFF;
	int16_t q_value = static_cast<int32_t>(std::stoi(q))		  &	0xFF;

	int32_t packed = (time_value << 16) | (i_value << 8) | q_value;

	return packed;
}

void receiver::setCSRBit()
{
	uint32_t data = 1;
	unsigned long long addr = baseAddr;

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
}

void receiver::sendTLMDataToXADC(int32_t data)
{
	unsigned long long addr = baseAddr + (0x04);

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
	printf("Data Transfered from RX to XADC : %d\n", data);
}

void receiver::resetCSRBit()
{
	printf("resetCSRBit Called \n");

	uint32_t data = 0;
	unsigned long long addr = baseAddr;

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
}

void receiver::read_csv()
{
	setCSRBit();

	ifstream file("/home/kabhi/work/personal/vectorWave/InputSignal/helloworldSignal.csv");

	if(!file.is_open())
	{
		cout<<"Receiver : cannot open signal file\n";
		return;
	}

	string line;
	getline(file, line);

	while(getline(file, line)) {
		stringstream ss(line);
		string t, i, q;
		getline(ss, t, ',');
		getline(ss, i, ',');
		getline(ss, q, ',');

		TLMPacket = bitPacking(t, i , q);
		sendTLMDataToXADC(TLMPacket);
	}
	file.close();

	resetCSRBit();
}

void receiver::op_thread()
{
	for(;;)
	{	
		wait(receiverPowerOn); // Directly feed the data to ADC
		printf("Signals Receiving \n");
		read_csv();	
		CSR = 0; // When the last sample is done 
	}
}

// called when a TLM transaction arrives for this target
void receiver::b_transport(tlm::tlm_generic_payload &trans, sc_time &delay)
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

	sc_time now = sc_time_stamp() + delay;

	if (cmd == tlm::TLM_READ_COMMAND) {
		static sc_time old_ts = SC_ZERO_TIME, diff;
		uint32_t v = 0;
		switch (addr)
		{
		case CSR_REGISTER:
			v = CSR;
			break;
		
		default:
			break;
		}
		memcpy(data, &v, len);

	// handle write commands
	} else if (cmd == tlm::TLM_WRITE_COMMAND) {
		static sc_time old_ts = SC_ZERO_TIME, diff;
		switch (addr) {

		case CSR_REGISTER:
			CSR = *(unsigned int *)data;
			printf("CSR Register Triggered\n");

			if(CSR == 1) // check 0x01 ADD
			{
				printf("Receiver Power On\n");
				receiverPowerOn.notify();
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

unsigned int receiver::transport_dbg(tlm::tlm_generic_payload &trans)
{
	unsigned int len = trans.get_data_length();
	return len;
}
