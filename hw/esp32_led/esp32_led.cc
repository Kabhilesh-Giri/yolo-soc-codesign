/*
 * esp32_led - a bus target that forwards LED writes to a real ESP32 over TCP.
 * Same structure as debugdev: a target socket, b_transport and transport_dbg.
 */

#include "esp32_led.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>

using namespace sc_core;

static const uint16_t ESP32_PORT      = 5025;
static const int      REPLY_TIMEOUT_S = 1;

esp32_led::esp32_led(sc_module_name name)
	: sc_module(name), socket("socket")
{
	socket.register_b_transport(this, &esp32_led::b_transport);
	socket.register_transport_dbg(this, &esp32_led::transport_dbg);

	connectToEsp32();
}

esp32_led::~esp32_led()
{
	if (fd >= 0)
		::close(fd);
}

// Called once at start-up: connect to the ESP32 named by ESP32_IP.
void esp32_led::connectToEsp32()
{
	const char *ip = std::getenv("ESP32_IP");
	if (ip == nullptr) {
		std::fprintf(stderr, "esp32_led: set ESP32_IP to the address the ESP32 prints "
				     "(for example ESP32_IP=10.71.163.178)\n");
		std::exit(EXIT_FAILURE);
	}

	sockaddr_in addr{};
	addr.sin_family = AF_INET;
	addr.sin_port = htons(ESP32_PORT);
	if (inet_pton(AF_INET, ip, &addr.sin_addr) != 1) {
		std::fprintf(stderr, "esp32_led: ESP32_IP is not an IPv4 address: %s\n", ip);
		std::exit(EXIT_FAILURE);
	}

	fd = ::socket(AF_INET, SOCK_STREAM, 0);   // '::' because 'socket' is also our TLM socket
	if (fd < 0) {
		std::perror("esp32_led: socket");
		std::exit(EXIT_FAILURE);
	}

	// A reply must arrive within 1 s. On Linux the send timeout also limits connect().
	timeval tv{};
	tv.tv_sec = REPLY_TIMEOUT_S;
	setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
	setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

	if (::connect(fd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) < 0) {
		std::fprintf(stderr, "esp32_led: cannot connect to %s:%u: %s\n", ip, ESP32_PORT,
			     errno == EINPROGRESS ? "timed out" : std::strerror(errno));
		std::exit(EXIT_FAILURE);
	}
	std::printf("esp32_led: connected to the ESP32 at %s:%u\n", ip, ESP32_PORT);
}

// Sends one command line and waits for one reply line (returned without '\n').
bool esp32_led::sendCommand(const std::string &command, std::string &reply)
{
	// TCP may take only part of the data, so keep sending until all of it is out.
	// MSG_NOSIGNAL: a closed connection gives an error instead of killing us with SIGPIPE.
	const std::string line = command + "\n";
	size_t sent = 0;
	while (sent < line.size()) {
		ssize_t n = ::send(fd, line.data() + sent, line.size() - sent, MSG_NOSIGNAL);
		if (n < 0) {
			if (errno == EINTR)
				continue;
			std::perror("esp32_led: send");
			return false;
		}
		sent += static_cast<size_t>(n);
	}

	// TCP is a byte stream: the reply can arrive in pieces, so read until '\n'.
	while (true) {
		size_t lf = pending.find('\n');
		if (lf != std::string::npos) {
			reply = pending.substr(0, lf);
			pending.erase(0, lf + 1);
			if (!reply.empty() && reply.back() == '\r')
				reply.pop_back();
			return true;
		}

		char buf[64];
		ssize_t n = ::recv(fd, buf, sizeof(buf), 0);
		if (n > 0) {
			pending.append(buf, static_cast<size_t>(n));
			continue;
		}
		if (n == 0) {
			std::fprintf(stderr, "esp32_led: the ESP32 closed the connection\n");
			return false;
		}
		if (errno == EINTR)
			continue;
		if (errno == EAGAIN || errno == EWOULDBLOCK) {
			std::fprintf(stderr, "esp32_led: no reply within %d s\n", REPLY_TIMEOUT_S);
			return false;
		}
		std::perror("esp32_led: recv");
		return false;
	}
}

void esp32_led::b_transport(tlm::tlm_generic_payload &trans, sc_time &delay)
{
	tlm::tlm_command cmd = trans.get_command();
	sc_dt::uint64 addr = trans.get_address();
	unsigned char *data = trans.get_data_ptr();
	unsigned int len = trans.get_data_length();
	unsigned char *byt = trans.get_byte_enable_ptr();
	unsigned int wid = trans.get_streaming_width();

	// Same checks as debugdev: no byte enables, no bursts.
	if (byt != 0) {
		trans.set_response_status(tlm::TLM_BYTE_ENABLE_ERROR_RESPONSE);
		return;
	}
	if (len > 4 || wid < len) {
		trans.set_response_status(tlm::TLM_BURST_ERROR_RESPONSE);
		return;
	}

	// Same 1 us register-access cost as the other devices.
	delay += sc_time(1, SC_US);
	wait(delay);
	delay = SC_ZERO_TIME;

	// Only one register exists. Anything else is a bus error for the firmware.
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

		// The simulation (and QEMU) wait here until the ESP32 answers.
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
	unsigned int len = trans.get_data_length();
	return len;
}
