#ifndef ESP32_LED_H
#define ESP32_LED_H

#include <cstdint>
#include <string>

#include <systemc>
#include <tlm>
#include "tlm_utils/simple_target_socket.h"

class esp32_led
: public sc_core::sc_module
{
public:
	tlm_utils::simple_target_socket<esp32_led> socket;

	esp32_led(sc_core::sc_module_name name);
	~esp32_led();

	virtual void b_transport(tlm::tlm_generic_payload &trans,
				 sc_core::sc_time &delay);
	virtual unsigned int transport_dbg(tlm::tlm_generic_payload &trans);

private:
	int fd = -1;            // the serial port (/dev/ttyUSB0)
	std::string pending;    // bytes received after the last complete line
	uint32_t ledValue = 0;  // last value written to the LED register

	void openSerial();
	bool readLine(std::string &line, int timeoutSeconds);
	bool sendCommand(const std::string &command, std::string &reply);
};

#endif
