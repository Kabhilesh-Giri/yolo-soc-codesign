/*
 * Receiver Device 
 */

#define SC_INCLUDE_DYNAMIC_PROCESSES

#include <inttypes.h>

#include <fstream>
#include <sstream>
#include <string>

#include "tlm_utils/simple_target_socket.h"
#include "receiver.h"

receiver::receiver(sc_module_name name)
	: sc_module(name), socket("socket")
{
	socket.register_b_transport(this, &receiver::b_transport);
	socket.register_transport_dbg(this, &receiver::transport_dbg);

	SC_THREAD(op_thread);
}

void receiver::read_csv()
{

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

		cout << "t=" << t << "ns I=" << i << " Q=" << q << endl;
	}
	file.close();
}

void receiver::op_thread()
{
	for(;;)
	{	
		wait(receiverPowerOn);
		printf("Signals Receiving \n");
		read_csv();
		// wait(receiverAPUPath | receiverADCPath);

		// if(receiverAPUPath.triggered())

		// if(receiverADCPath.triggered())

		wait(5, SC_MS); // Wait for 5 ms when it gets notified

		CSR = 0; // Operation has concluded reset the CSR Bit

		cout << "Operation has concluded resetted Receiver CSR bit \n";
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

	// compute current time (incl. any quantum offset if no sync above)
	sc_time now = sc_time_stamp() + delay;

	// handle reads commands
	if (cmd == tlm::TLM_READ_COMMAND) {
		static sc_time old_ts = SC_ZERO_TIME, diff;
		uint32_t v = 0;
		switch (addr)
		{
		case CSR_REGISTER:
			v = CSR;
			break;
		
		// case PATH_APU: // 0x01
		// 	break;	

		// case PATH_ADC: // 0x02
		// 	break;	
			
		// default:
		// 	break;
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

		// 	else if(CSR == 2) // check 0x02 SUB
		// 	{
		// 		printf("Vector Sub Triggered\n");
		// 		receiverADCPath.notify();
		// 	}
		// 	else
		// 		CSR = 0; // To Stop Polling 
			break;
        case PATH_APU: // 0x01
			printf("PATH_APU Register Triggered\n");
			printf("Data Passed %u\n", *(unsigned int *)data);
			break;	
            
		case PATH_ADC: // 0x02
			printf("PATH_ADC Register Triggered\n");
			printf("Data Passed %u\n", *(unsigned int *)data);
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
