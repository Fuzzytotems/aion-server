#include <gtest/gtest.h>

#include <atomic>
#include <map>
#include <thread>
#include <vector>

#include "NetworkTestUtils.h"
#include "aion/commons/utils/Exception.h"

using namespace nettest;
using namespace aion::commons;

namespace {

std::vector<uint8_t> bytes(std::initializer_list<uint8_t> list) {
	return list;
}

void append(std::vector<uint8_t>& target, const std::vector<uint8_t>& source) {
	target.insert(target.end(), source.begin(), source.end());
}

} // namespace

TEST(AConnectionTest, InitializedRunsBeforeReadingAndCanSendPackets) {
	TestServer server;
	server.probe->onInitialized = [](TestConnection& con) { con.sendPacket(makePacket({'h', 'i'})); };
	server.start();
	TestClient client(server.port);
	client.send(frame({1, 2, 3})); // sent immediately, but processed only after initialized()

	auto greeting = client.readFrame();
	ASSERT_TRUE(greeting);
	EXPECT_EQ(*greeting, bytes({'h', 'i'}));
	ASSERT_TRUE(waitUntil([&] { return server.probe->receivedCount() == 1; }));
	EXPECT_EQ(server.probe->initializedCount, 1);
	EXPECT_FALSE(server.probe->processDataBeforeInitialized);
	EXPECT_EQ(server.connection(0)->getIP(), "127.0.0.1");
}

TEST(AConnectionTest, FramingHandlesCoalescedAndSplitPackets) {
	TestServer server;
	server.start();
	TestClient client(server.port);

	// several packets in one write
	std::vector<uint8_t> data;
	append(data, frame({0x11}));
	append(data, frame({0x21, 0x22}));
	append(data, frame({0x31, 0x32, 0x33}));
	// followed by the start of a packet whose rest arrives later
	std::vector<uint8_t> big(300);
	for (size_t i = 0; i < big.size(); i++)
		big[i] = static_cast<uint8_t>(i);
	std::vector<uint8_t> bigFrame = frame(big);
	data.insert(data.end(), bigFrame.begin(), bigFrame.begin() + 1); // only the first byte of the length
	client.send(data);
	ASSERT_TRUE(waitUntil([&] { return server.probe->receivedCount() == 3; }));

	// the rest in small pieces, each delivered separately
	size_t offset = 1;
	for (size_t chunk : {1, 5, 100, 50}) {
		client.send(std::span(bigFrame).subspan(offset, chunk));
		offset += chunk;
		std::this_thread::sleep_for(10ms);
		EXPECT_EQ(server.probe->receivedCount(), 3u);
	}
	client.send(std::span(bigFrame).subspan(offset));
	ASSERT_TRUE(waitUntil([&] { return server.probe->receivedCount() == 4; }));

	auto received = server.probe->receivedCopy();
	EXPECT_EQ(received[0], bytes({0x11}));
	EXPECT_EQ(received[1], bytes({0x21, 0x22}));
	EXPECT_EQ(received[2], bytes({0x31, 0x32, 0x33}));
	EXPECT_EQ(received[3], big);
	EXPECT_EQ(server.probe->disconnectCount, 0);
}

TEST(AConnectionTest, PacketExactlyFillingTheReadBufferIsProcessed) {
	TestServer server;
	server.rbSize = 64;
	server.start();
	TestClient client(server.port);
	std::vector<uint8_t> payload(62, 0xAB);
	client.send(frame(payload));
	client.send(frame(payload));
	ASSERT_TRUE(waitUntil([&] { return server.probe->receivedCount() == 2; }));
	EXPECT_EQ(server.probe->receivedCopy()[1], payload);
	EXPECT_EQ(server.probe->disconnectCount, 0);
}

TEST(AConnectionTest, SendPacketFromManyThreadsKeepsPerThreadOrder) {
	TestServer server;
	server.start(4);
	TestClient client(server.port);
	auto connection = server.connection(0);
	ASSERT_TRUE(connection);

	constexpr int threadCount = 8;
	constexpr int packetsPerThread = 500;
	std::vector<std::thread> senders;
	for (int t = 0; t < threadCount; t++) {
		senders.emplace_back([&, t] {
			for (int i = 0; i < packetsPerThread; i++)
				connection->sendPacket(makePacket({static_cast<uint8_t>(t), static_cast<uint8_t>(i & 0xFF), static_cast<uint8_t>(i >> 8)}));
		});
	}

	std::map<int, int> nextExpected;
	for (int n = 0; n < threadCount * packetsPerThread; n++) {
		auto packet = client.readFrame();
		ASSERT_TRUE(packet) << "packet " << n;
		ASSERT_EQ(packet->size(), 3u);
		int t = (*packet)[0];
		int i = (*packet)[1] | ((*packet)[2] << 8);
		ASSERT_EQ(i, nextExpected[t]) << "thread " << t;
		nextExpected[t]++;
	}
	for (auto& sender : senders)
		sender.join();
}

TEST(AConnectionTest, SamePacketInstanceCanBeBroadcastToSeveralConnections) {
	TestServer server;
	server.start(4);
	TestClient client1(server.port);
	TestClient client2(server.port);
	auto con1 = server.connection(0);
	auto con2 = server.connection(1);
	ASSERT_TRUE(con1 && con2);

	auto broadcast = makePacket({'a', 'l', 'l'});
	for (int i = 0; i < 100; i++) {
		con1->sendPacket(broadcast);
		con2->sendPacket(broadcast);
	}
	for (int i = 0; i < 100; i++) {
		ASSERT_EQ(client1.readFrame(), broadcast->payload);
		ASSERT_EQ(client2.readFrame(), broadcast->payload);
	}
}

TEST(AConnectionTest, CloseWithPacketSendsItLastThenDisconnects) {
	TestServer server;
	server.start();
	TestClient client(server.port);
	auto connection = server.connection(0);
	ASSERT_TRUE(connection);

	connection->sendPacket(makePacket({1}));
	connection->close(makePacket({'e', 'n', 'd'}));
	connection->sendPacket(makePacket({2})); // ignored: close is pending
	connection->close(makePacket({3})); // ignored: close is pending

	std::vector<std::vector<uint8_t>> packets;
	while (auto packet = client.readFrame(2s))
		packets.push_back(*packet);
	ASSERT_FALSE(packets.empty());
	EXPECT_EQ(packets.back(), bytes({'e', 'n', 'd'}));
	EXPECT_LE(packets.size(), 2u); // packet 1 may have been sent before close cleared the queue
	ASSERT_TRUE(waitUntil([&] { return server.probe->disconnectCount == 1; }));
	EXPECT_TRUE(connection->isClosed());
	EXPECT_FALSE(connection->isPendingClose());
	EXPECT_EQ(server.server->getActiveConnectionCount(), 0u);
}

TEST(AConnectionTest, CloseWithoutPacketFlushesTheQueueFirst) {
	TestServer server;
	server.start();
	TestClient client(server.port);
	auto connection = server.connection(0);
	ASSERT_TRUE(connection);

	for (int i = 0; i < 200; i++)
		connection->sendPacket(makePacket(std::vector<uint8_t>(100, static_cast<uint8_t>(i))));
	connection->close();
	for (int i = 0; i < 200; i++) {
		auto packet = client.readFrame();
		ASSERT_TRUE(packet) << i;
		EXPECT_EQ((*packet)[0], static_cast<uint8_t>(i));
	}
	EXPECT_TRUE(client.waitForClose());
	ASSERT_TRUE(waitUntil([&] { return server.probe->disconnectCount == 1; }));
}

TEST(AConnectionTest, OnDisconnectOnceWhenClientCloses) {
	TestServer server;
	server.start();
	TestClient client(server.port);
	auto connection = server.connection(0);
	ASSERT_TRUE(connection);
	EXPECT_EQ(server.server->getActiveConnectionCount(), 1u);

	client.close();
	ASSERT_TRUE(waitUntil([&] { return server.probe->disconnectCount == 1; }));
	EXPECT_TRUE(connection->isClosed());
	connection->close();
	connection->close(makePacket({1}));
	connection->sendPacket(makePacket({1}));
	std::this_thread::sleep_for(50ms);
	EXPECT_EQ(server.probe->disconnectCount, 1);
	EXPECT_FALSE(server.probe->onDisconnectOnIoThread);
	EXPECT_EQ(server.server->getActiveConnectionCount(), 0u);
}

TEST(AConnectionTest, OnDisconnectOnceWhenServerClosesConcurrently) {
	TestServer server;
	server.start(4);
	TestClient client(server.port);
	auto connection = server.connection(0);
	ASSERT_TRUE(connection);

	std::vector<std::thread> closers;
	for (int i = 0; i < 8; i++)
		closers.emplace_back([&, i] {
			if (i % 2)
				connection->close();
			else
				connection->close(makePacket({static_cast<uint8_t>(i)}));
		});
	for (auto& closer : closers)
		closer.join();
	EXPECT_TRUE(client.waitForClose());
	ASSERT_TRUE(waitUntil([&] { return server.probe->disconnectCount == 1; }));
	std::this_thread::sleep_for(50ms);
	EXPECT_EQ(server.probe->disconnectCount, 1);
}

TEST(AConnectionTest, EmptyPacketClosesConnection) {
	LogCapture& logs = LogCapture::instance();
	logs.clear();
	TestServer server;
	server.start();
	TestClient client(server.port);
	client.send(frame({7}));
	client.send(bytes({0x02, 0x00, 0x55})); // declared size 2: no opcode
	EXPECT_TRUE(client.waitForClose());
	ASSERT_TRUE(waitUntil([&] { return server.probe->disconnectCount == 1; }));
	EXPECT_EQ(server.probe->receivedCount(), 1u);
	EXPECT_TRUE(logs.contains("warning|com.aionemu.commons.network.Dispatcher|Received empty packet without opcode from TestConnection 127.0.0.1, content: "));
}

TEST(AConnectionTest, TooSmallLengthPrefixClosesConnection) {
	TestServer server;
	server.start();
	TestClient client(server.port);
	client.send(bytes({0x00, 0x00, 0x01, 0x02}));
	EXPECT_TRUE(client.waitForClose());
	ASSERT_TRUE(waitUntil([&] { return server.probe->disconnectCount == 1; }));
	EXPECT_EQ(server.probe->receivedCount(), 0u);
}

TEST(AConnectionTest, PacketLargerThanReadBufferClosesConnection) {
	LogCapture& logs = LogCapture::instance();
	logs.clear();
	TestServer server;
	server.rbSize = 64;
	server.start();
	TestClient client(server.port);
	client.send(bytes({0x41, 0x00, 0x01})); // 65 bytes can never fit into the 64 byte read buffer
	EXPECT_TRUE(client.waitForClose());
	ASSERT_TRUE(waitUntil([&] { return server.probe->disconnectCount == 1; }));
	EXPECT_EQ(server.probe->receivedCount(), 0u);
	EXPECT_TRUE(logs.contains("exceeds the read buffer size of 64 bytes"));
}

TEST(AConnectionTest, ProcessDataReturningFalseClosesImmediately) {
	TestServer server;
	std::atomic<int> processed = 0;
	server.probe->onPacket = [&](TestConnection&, const std::vector<uint8_t>& payload) {
		processed++;
		return payload[0] != 0xFF;
	};
	server.start();
	TestClient client(server.port);
	std::vector<uint8_t> data;
	append(data, frame({1}));
	append(data, frame({0xFF}));
	append(data, frame({2})); // in the same read, must not be processed anymore
	client.send(data);
	EXPECT_TRUE(client.waitForClose());
	ASSERT_TRUE(waitUntil([&] { return server.probe->disconnectCount == 1; }));
	EXPECT_EQ(processed, 2);
}

TEST(AConnectionTest, ProcessDataThrowingClosesAndLogs) {
	LogCapture& logs = LogCapture::instance();
	logs.clear();
	TestServer server;
	std::atomic<int> processed = 0;
	server.probe->onPacket = [&](TestConnection&, const std::vector<uint8_t>&) -> bool {
		processed++;
		throw utils::IllegalStateException("bad data");
	};
	server.start();
	TestClient client(server.port);
	std::vector<uint8_t> data;
	append(data, frame({0xAA, 0xBB}));
	append(data, frame({2}));
	client.send(data);
	EXPECT_TRUE(client.waitForClose());
	ASSERT_TRUE(waitUntil([&] { return server.probe->disconnectCount == 1; }));
	EXPECT_EQ(processed, 1);
	EXPECT_TRUE(logs.contains("error|com.aionemu.commons.network.Dispatcher|Error parsing input from TestConnection 127.0.0.1, packet size: 2, content: "));
	EXPECT_TRUE(logs.contains("bad data"));
}

TEST(AConnectionTest, ProcessDataMayCloseWithPacket) {
	TestServer server;
	server.probe->onPacket = [&](TestConnection& con, const std::vector<uint8_t>&) {
		con.close(makePacket({'n', 'o'}));
		return true;
	};
	server.start();
	TestClient client(server.port);
	client.send(frame({1}));
	EXPECT_EQ(client.readFrame(), bytes({'n', 'o'}));
	EXPECT_TRUE(client.waitForClose());
	ASSERT_TRUE(waitUntil([&] { return server.probe->disconnectCount == 1; }));
}

TEST(AConnectionTest, PacketExceedingWriteBufferDisconnects) {
	LogCapture& logs = LogCapture::instance();
	logs.clear();
	TestServer server;
	server.wbSize = 32;
	server.start();
	TestClient client(server.port);
	auto connection = server.connection(0);
	ASSERT_TRUE(connection);
	connection->sendPacket(makePacket(std::vector<uint8_t>(100, 1)));
	EXPECT_TRUE(client.waitForClose());
	ASSERT_TRUE(waitUntil([&] { return server.probe->disconnectCount == 1; }));
	EXPECT_TRUE(logs.contains("Error writing data for TestConnection 127.0.0.1"));
}

TEST(AConnectionTest, PendingCloseIsForcedAfterTwoSeconds) {
	TestServer server;
	server.wbSize = 8192;
	server.start();
	TestClient client(server.port); // never reads, so the socket buffers fill up
	auto connection = server.connection(0);
	ASSERT_TRUE(connection);

	auto packet = makePacket(std::vector<uint8_t>(8000, 7));
	for (int i = 0; i < 40000; i++) // ~320 MB, far more than the socket buffers hold
		connection->sendPacket(packet);
	auto start = std::chrono::steady_clock::now();
	connection->close();
	EXPECT_TRUE(connection->isPendingClose());
	ASSERT_TRUE(waitUntil([&] { return server.probe->disconnectCount == 1; }, 10s));
	auto elapsed = std::chrono::steady_clock::now() - start;
	EXPECT_GE(elapsed, 1900ms);
	EXPECT_LT(elapsed, 5s); // generous for loaded machines, the lower bound proves the deadline
}

TEST(AConnectionTest, ConnectionsCanBeFormattedWithFmt) {
	TestServer server;
	server.start();
	TestClient client(server.port);
	auto connection = server.connection(0);
	ASSERT_TRUE(connection);
	EXPECT_EQ(fmt::format("{}", *connection), "TestConnection 127.0.0.1");
}

TEST(AConnectionTest, CloseWithPacketInFactoryIsCompletedAfterRegistration) {
	auto probe = std::make_shared<Probe>();
	std::atomic<bool> disconnectedBeforeInitialized = false;
	network::ServerCfg cfg{{"127.0.0.1", 0}, "test clients", [&](asio::ip::tcp::socket socket, network::NioServer& nioServer) -> std::shared_ptr<network::AConnectionBase> {
		auto connection = std::make_shared<TestConnection>(std::move(socket), nioServer, probe);
		connection->close(makePacket({'n', 'o'})); // reject the client with a packet
		std::this_thread::sleep_for(100ms); // other IO threads must not disconnect the unregistered connection meanwhile
		return connection;
	}};
	probe->onInitialized = [&](TestConnection&) {
		if (probe->disconnectCount > 0)
			disconnectedBeforeInitialized = true;
	};
	network::NioServer server(4, {cfg});
	server.connect();
	TestClient client(server.getBoundAddresses().at(0).port);
	EXPECT_EQ(client.readFrame(), bytes({'n', 'o'}));
	EXPECT_TRUE(client.waitForClose());
	ASSERT_TRUE(waitUntil([&] { return probe->disconnectCount == 1; }));
	EXPECT_EQ(probe->initializedCount, 1);
	EXPECT_FALSE(disconnectedBeforeInitialized);
	EXPECT_EQ(server.getActiveConnectionCount(), 0u);
}

TEST(AConnectionTest, CloseInInitializedDisconnectsOnlyAfterInitializedReturned) {
	TestServer server;
	std::atomic<bool> initializedReturned = false;
	std::atomic<bool> disconnectedTooEarly = false;
	server.probe->onInitialized = [&](TestConnection& con) {
		con.close(makePacket({'b', 'y', 'e'}));
		std::this_thread::sleep_for(100ms);
		if (server.probe->disconnectCount > 0)
			disconnectedTooEarly = true;
		initializedReturned = true;
	};
	server.start(4);
	TestClient client(server.port);
	EXPECT_EQ(client.readFrame(), bytes({'b', 'y', 'e'}));
	EXPECT_TRUE(client.waitForClose());
	ASSERT_TRUE(waitUntil([&] { return server.probe->disconnectCount == 1; }));
	EXPECT_TRUE(initializedReturned);
	EXPECT_FALSE(disconnectedTooEarly);
	EXPECT_EQ(server.server->getActiveConnectionCount(), 0u);
}

TEST(AConnectionTest, OutboundConnectionClosedBeforeRegistrationIsDisconnectedAfterIt) {
	TestServer target;
	target.start();
	auto probe = std::make_shared<Probe>();
	network::NioServer server(1, {});
	server.connect();
	auto connection = std::make_shared<TestConnection>(server.openSocket({"127.0.0.1", target.port}), server, probe);
	connection->close();
	std::this_thread::sleep_for(200ms); // the IO thread must not disconnect the unregistered connection meanwhile
	server.registerConnection(connection);
	ASSERT_TRUE(waitUntil([&] { return probe->disconnectCount == 1; }));
	EXPECT_EQ(probe->initializedCount, 1);
	EXPECT_TRUE(connection->isClosed());
	EXPECT_EQ(server.getActiveConnectionCount(), 0u);
}

TEST(AConnectionTest, ClosePacketIsDeliveredWhenThePeerHalfClosesWhileItIsPending) {
	TestServer server;
	server.wbSize = 8192;
	server.start();
	TestClient client(server.port);
	auto connection = server.connection(0);
	ASSERT_TRUE(connection);

	// fill the socket buffers, so the close packet cannot be written right away
	auto packet = makePacket(std::vector<uint8_t>(8000, 5));
	for (int i = 0; i < 2000; i++)
		connection->sendPacket(packet);
	std::this_thread::sleep_for(300ms);
	connection->close(makePacket({'b', 'y', 'e'})); // e.g. from a PacketProcessor thread
	// the client half-closes: the pending read of the server completes with EOF while the close packet waits for the write
	client.getSocket().shutdown(asio::ip::tcp::socket::shutdown_send);
	std::this_thread::sleep_for(200ms);

	std::optional<std::vector<uint8_t>> last;
	while (auto frame = client.readFrame(1s))
		last = std::move(frame);
	ASSERT_TRUE(last);
	EXPECT_EQ(*last, bytes({'b', 'y', 'e'}));
	ASSERT_TRUE(waitUntil([&] { return server.probe->disconnectCount == 1; }));
}
