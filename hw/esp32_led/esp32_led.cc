/*
 * esp32_led - a bus target that forwards LED writes to an ESP32 over USB serial.
 * Port from ESP32_TTY (default /dev/ttyUSB0), 115200 baud, 8N1, raw.
 */

#include "esp32_led.h"

#include <fcntl.h>
#include <termios.h>
#include <unistd.h>

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>

using namespace sc_core;

static const int REPLY_TIMEOUT_S = 1;   // per command
static const int READY_TIMEOUT_S = 5;   // ESP32 boot after the port opens

esp32_led::esp32_led(sc_module_name name)
	: sc_module(name), socket("socket")
{
	socket.register_b_transport(this, &esp32_led::b_transport);
	socket.register_transport_dbg(this, &esp32_led::transport_dbg);

	openSerial();
}

esp32_led::~esp32_led()
{
	if (fd >= 0)
		::close(fd);
}

// Called once at start-up: open the port, set 115200 8N1 raw, wait for "READY".
void esp32_led::openSerial()
{
	const char *env = std::getenv("ESP32_TTY");
	const char *tty = env ? env : "/dev/ttyUSB0";

	fd = ::open(tty, O_RDWR | O_NOCTTY);
	if (fd < 0) {
		std::fprintf(stderr, "esp32_led: cannot open %s: %s\n"
			     "  (is the board attached? usbipd attach --wsl --busid <id>)\n",
			     tty, std::strerror(errno));
		std::exit(EXIT_FAILURE);
	}

	termios tio{};
	if (tcgetattr(fd, &tio) != 0) {
		std::perror("esp32_led: tcgetattr");
		std::exit(EXIT_FAILURE);
	}
	cfmakeraw(&tio);                  // no echo, no line editing, 8 data bits
	cfsetispeed(&tio, B115200);
	cfsetospeed(&tio, B115200);
	tio.c_cflag |= (CLOCAL | CREAD);  // ignore modem lines, enable the receiver
	tio.c_cflag &= ~CSTOPB;           // 1 stop bit
	tio.c_cflag &= ~CRTSCTS;          // no hardware flow control
	tio.c_cc[VMIN]  = 0;              // read() returns when data arrives...
	tio.c_cc[VTIME] = 10;             // ...or after 1.0 s (0 = timeout)
	if (tcsetattr(fd, TCSANOW, &tio) != 0) {
		std::perror("esp32_led: tcsetattr");
		std::exit(EXIT_FAILURE);
	}

	// Opening the port usually resets the ESP32 (auto-reset circuit). Its boot ROM
	// prints text first, so skip every line until "READY" arrives.
	std::time_t start = std::time(nullptr);
	std::string line;
	while (std::time(nullptr) - start < READY_TIMEOUT_S) {
		if (readLine(line, 1) && line == "READY") {
			std::printf("esp32_led: ESP32 ready on %s\n", tty);
			return;
		}
	}

	// No reset happened (the board was already running): ask it directly.
	std::string reply;
	if (sendCommand("LED 0", reply) && reply == "OK") {
		std::printf("esp32_led: ESP32 answering on %s\n", tty);
		return;
	}
	std::fprintf(stderr, "esp32_led: no READY or OK from the ESP32 on %s\n", tty);
	std::exit(EXIT_FAILURE);
}

// Reads one line (returned without '\n' or '\r'). False on timeout or error.
bool esp32_led::readLine(std::string &line, int timeoutSeconds)
{
	std::time_t start = std::time(nullptr);
	while (true) {
		size_t lf = pending.find('\n');
		if (lf != std::string::npos) {
			line = pending.substr(0, lf);
			pending.erase(0, lf + 1);
			if (!line.empty() && line.back() == '\r')
				line.pop_back();
			return true;
		}
		if (std::time(nullptr) - start >= timeoutSeconds)
			return false;

		char buf[64];
		ssize_t n = ::read(fd, buf, sizeof(buf));
		if (n > 0) {
			pending.append(buf, static_cast<size_t>(n));
			continue;
		}
		if (n == 0)                // VTIME expired with no data: keep waiting
			continue;
		if (errno == EINTR)
			continue;
		std::perror("esp32_led: read");
		return false;
	}
}

// Sends one command line and waits for one reply line.
bool esp32_led::sendCommand(const std::string &command, std::string &reply)
{
	const std::string out = command + "\n";
	size_t sent = 0;
	while (sent < out.size()) {
		ssize_t n = ::write(fd, out.data() + sent, out.size() - sent);
		if (n < 0) {
			if (errno == EINTR)
				continue;
			std::perror("esp32_led: write");
			return false;
		}
		sent += static_cast<size_t>(n);
	}
	tcdrain(fd);                       // wait until the bytes have left the port

	if (!readLine(reply, REPLY_TIMEOUT_S)) {
		std::fprintf(stderr, "esp32_led: no reply within %d s\n", REPLY_TIMEOUT_S);
		return false;
	}
	return true;
}

void esp32_led::b_transport(tlm::tlm_generic_payload &trans, sc_time &delay)
{
	tlm::tlm_command cmd = trans.get_command();
	sc_dt::uint64 addr = trans.get_address();
	unsigned char *data = trans.get_data_ptr();
	unsigned int len = trans.get_data_length();
	unsigned char *byt = trans.get_byte_enable_ptr();
	unsigned int wid = trans.get_streaming_width();

	if (byt != 0) {
		trans.set_response_status(tlm::TLM_BYTE_ENABLE_ERROR_RESPONSE);
		return;
	}
	if (len > 4 || wid < len) {
		trans.set_response_status(tlm::TLM_BURST_ERROR_RESPONSE);
		return;
	}

	delay += sc_time(1, SC_US);
	wait(delay);
	delay = SC_ZERO_TIME;

	if (addr != 0x00) {
		trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
		return;
	}

	if (cmd == tlm::TLM_READ_COMMAND) {
		std::memcpy(data, &ledValue, len);
		trans.set_response_status(tlm::TLM_OK_RESPONSE);

	} else if (cmd == tlm::TLM_WRITE_COMMAND) {
		uint32_t value = 0;
		std::memcpy(&value, data, len);
		const std::string command = (value & 1) ? "LED 1" : "LED 0";

		std::string reply;
		if (sendCommand(command, reply) && reply == "OK") {
			ledValue = value & 1;
			std::printf("esp32_led: %s -> OK\n", command.c_str());
			trans.set_response_status(tlm::TLM_OK_RESPONSE);
		} else {
			std::fprintf(stderr, "esp32_led: %s failed (reply '%s')\n",
				     command.c_str(), reply.c_str());
			trans.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
		}

	} else {
		trans.set_response_status(tlm::TLM_COMMAND_ERROR_RESPONSE);
	}
}

unsigned int esp32_led::transport_dbg(tlm::tlm_generic_payload &trans)
{
	return trans.get_data_length();
}
