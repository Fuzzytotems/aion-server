#include <array>
#include <cstdint>
#include <string>
#include <thread>
#include <vector>

#include <asio/io_context.hpp>
#include <asio/ip/tcp.hpp>
#include <asio/read.hpp>
#include <asio/write.hpp>
#include <gtest/gtest.h>

#include "aion/commons/database/Connection.h"

using namespace aion::commons::database;

// Connection tests against a fake server on the loopback interface (no database server needed).

namespace {

/** Speaks just enough of the MariaDB protocol to accept one login, advertising no TLS support. It closes the connection on the next command. */
class NoTlsFakeServer {
public:
	NoTlsFakeServer() : acceptor(context, {asio::ip::address_v4::loopback(), 0}) {
		worker = std::thread([this] {
			try {
				acceptor.accept(socket);
				writePacket(0, handshake());
				readPacket(); // handshake response (credentials are not checked)
				writePacket(2, {0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00}); // OK, autocommit
				readPacket(); // first command (or COM_QUIT): not answered
			} catch (const std::exception&) {
				// client disconnected or server closed
			}
			std::error_code ec;
			socket.close(ec);
		});
	}

	~NoTlsFakeServer() {
		std::error_code ec;
		acceptor.close(ec);
		socket.shutdown(asio::ip::tcp::socket::shutdown_both, ec);
		worker.join();
	}

	uint16_t port() const { return acceptor.local_endpoint().port(); }

private:
	static std::vector<uint8_t> handshake() {
		std::vector<uint8_t> p{10};
		for (char c : std::string("5.5.5-10.11.0-MariaDB"))
			p.push_back(static_cast<uint8_t>(c));
		p.push_back(0);
		p.insert(p.end(), {1, 0, 0, 0});						// connection id
		p.insert(p.end(), {'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h'}); // scramble, part 1
		p.push_back(0);
		// capabilities (lower): LONG_PASSWORD, FOUND_ROWS, LONG_FLAG, CONNECT_WITH_DB, PROTOCOL_41, TRANSACTIONS, SECURE_CONNECTION - no SSL
		p.insert(p.end(), {0x0F, 0xA2});
		p.push_back(45);								// utf8mb4_general_ci
		p.insert(p.end(), {0x02, 0x00});		// status: autocommit
		p.insert(p.end(), {0x0A, 0x00});		// capabilities (upper): MULTI_RESULTS, PLUGIN_AUTH
		p.push_back(21);								// auth data length
		p.insert(p.end(), 10, uint8_t{0}); // reserved
		p.insert(p.end(), {'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't', 0}); // scramble, part 2
		for (char c : std::string("mysql_native_password"))
			p.push_back(static_cast<uint8_t>(c));
		p.push_back(0);
		return p;
	}

	void writePacket(uint8_t sequence, const std::vector<uint8_t>& payload) {
		std::vector<uint8_t> packet{static_cast<uint8_t>(payload.size()), static_cast<uint8_t>(payload.size() >> 8),
			static_cast<uint8_t>(payload.size() >> 16), sequence};
		packet.insert(packet.end(), payload.begin(), payload.end());
		asio::write(socket, asio::buffer(packet));
	}

	void readPacket() {
		std::array<uint8_t, 4> header;
		asio::read(socket, asio::buffer(header));
		std::vector<uint8_t> payload(header[0] | header[1] << 8 | header[2] << 16);
		asio::read(socket, asio::buffer(payload));
	}

	asio::io_context context;
	asio::ip::tcp::acceptor acceptor;
	asio::ip::tcp::socket socket{context};
	std::thread worker;
};

} // namespace

TEST(ConnectionTest, SslModeRequiredFailsWithoutServerTls) {
	NoTlsFakeServer server;
	ConnectionProperties properties = ConnectionProperties::parse(
		"jdbc:mysql://127.0.0.1:" + std::to_string(server.port()) + "/test?sslMode=REQUIRED&connectTimeout=5000", "root", "secret");
	try {
		Connection::open(properties);
		FAIL() << "connected without TLS";
	} catch (const SQLException& e) {
		EXPECT_EQ(e.getSQLState(), "08001") << e.what() << " (" << e.getErrorCode() << ")";
		EXPECT_NE(std::string(e.what()).find("SSL"), std::string::npos) << e.what();
	}
}

TEST(ConnectionTest, SslModePreferredFallsBackToPlainConnection) {
	NoTlsFakeServer server;
	ConnectionProperties properties = ConnectionProperties::parse(
		"jdbc:mysql://127.0.0.1:" + std::to_string(server.port()) + "/test?connectTimeout=5000", "root", "secret");
	// the fake server accepts the login but cannot answer the session setup query
	try {
		Connection::open(properties);
		FAIL();
	} catch (const SQLException& e) {
		EXPECT_EQ(e.getSQLState(), "08S01") << e.what() << " (" << e.getErrorCode() << ")"; // lost connection after a successful login
	}
}
