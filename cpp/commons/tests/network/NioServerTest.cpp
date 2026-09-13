#include <gtest/gtest.h>

#include <atomic>
#include <set>
#include <thread>
#include <vector>

#include "NetworkTestUtils.h"
#include "aion/commons/network/SocketException.h"
#include "aion/commons/utils/Exception.h"

using namespace nettest;
using namespace aion::commons;
using network::NioServer;
using network::ServerCfg;

namespace {

/** @return a port on 127.0.0.1 that nothing listens on (bound and released again) */
uint16_t unusedPort() {
	asio::io_context io;
	asio::ip::tcp::acceptor acceptor(io, {asio::ip::make_address_v4("127.0.0.1"), 0});
	return acceptor.local_endpoint().port();
}

} // namespace

TEST(NioServerTest, ConnectLogsListeningAddresses) {
	LogCapture& logs = LogCapture::instance();
	logs.clear();
	TestServer server;
	server.start();
	EXPECT_TRUE(logs.contains("info|com.aionemu.commons.network.NioServer|Listening on 127.0.0.1:0 for test clients"));
	EXPECT_NE(server.port, 0);
	EXPECT_EQ(server.server->getBoundAddresses().at(0).host, "127.0.0.1");
	EXPECT_THROW(server.server->connect(), utils::IllegalStateException);
}

TEST(NioServerTest, AnyLocalAddressInfo) {
	ServerCfg cfg{{"0.0.0.0", 2106}, "Aion game clients", {}};
	EXPECT_TRUE(cfg.isAnyLocalAddress());
	EXPECT_EQ(cfg.getAddressInfo(), "all addresses on port 2106");
	EXPECT_EQ(cfg.getPort(), 2106);
	ServerCfg local{{"127.0.0.1", 9014}, "game servers", {}};
	EXPECT_FALSE(local.isAnyLocalAddress());
	EXPECT_EQ(local.getAddressInfo(), "127.0.0.1:9014");
	EXPECT_EQ(local.getIP(), "127.0.0.1");
}

TEST(NioServerTest, BindFailureThrowsIOException) {
	TestServer first;
	first.start();

	ServerCfg cfg{{"127.0.0.1", first.port}, "duplicate", {}};
	NioServer second(1, {cfg});
	try {
		second.connect();
		FAIL() << "expected IOException";
	} catch (const utils::IOException& e) {
		EXPECT_TRUE(std::string_view(e.what()).starts_with("Could not open server socket: ")) << e.what();
	}
	EXPECT_EQ(second.getActiveConnectionCount(), 0u);
}

TEST(NioServerTest, FactoryReturningNullClosesTheSocket) {
	TestServer server;
	server.rejectConnections = true;
	server.start(0);
	TestClient client(server.port);
	EXPECT_TRUE(client.waitForClose());
	EXPECT_EQ(server.probe->initializedCount, 0);
	EXPECT_EQ(server.server->getActiveConnectionCount(), 0u);
}

TEST(NioServerTest, SingleThreadRunsAllNetworkCodeOnOneThread) {
	TestServer server;
	std::mutex mutex;
	std::set<std::thread::id> threadIds;
	std::set<std::string> threadNames;
	auto record = [&] {
		std::lock_guard lock(mutex);
		threadIds.insert(std::this_thread::get_id());
		threadNames.insert(utils::concurrent::getCurrentThreadName());
	};
	std::atomic<int> factoryCalls = 0;
	std::atomic<int> writeDataCalls = 0;
	server.onFactory = [&] {
		record();
		factoryCalls++;
	};
	server.probe->onInitialized = [&](TestConnection&) { record(); };
	server.probe->onPacket = [&](TestConnection& con, const std::vector<uint8_t>& payload) {
		record();
		con.sendPacket(makePacket(payload)); // echo, so writeData runs as well
		return true;
	};
	server.probe->onWriteData = [&](TestConnection&) {
		record();
		writeDataCalls++;
	};
	server.start(0);
	std::vector<std::unique_ptr<TestClient>> clients;
	for (int i = 0; i < 4; i++) {
		clients.push_back(std::make_unique<TestClient>(server.port));
		clients.back()->send(frame({1}));
	}
	for (auto& client : clients)
		EXPECT_EQ(client->readFrame(), std::vector<uint8_t>({1}));
	ASSERT_TRUE(waitUntil([&] { return server.probe->initializedCount == 4 && server.server->getActiveConnectionCount() == 4; }));
	std::lock_guard lock(mutex);
	EXPECT_EQ(factoryCalls, 4);
	EXPECT_GE(writeDataCalls, 4);
	EXPECT_EQ(threadIds.size(), 1u);
	EXPECT_EQ(threadNames, std::set<std::string>{"AcceptReadWrite Dispatcher"});
}

TEST(NioServerTest, OutboundConnectionIsRegisteredLikeInJava) {
	TestServer loginServer;
	loginServer.probe->onInitialized = [](TestConnection& con) { con.sendPacket(makePacket({'l', 's'})); };
	loginServer.start();

	auto probe = std::make_shared<Probe>();
	NioServer gameServer(1, {});
	gameServer.connect();

	// Java LoginServer.connect: lsCon = new LoginServerConnection(sc, d); d.register(sc, OP_READ, lsCon); lsCon.initialized();
	std::shared_ptr<TestConnection> lsCon;
	std::atomic<bool> fieldSetInInitialized = false;
	std::atomic<bool> fieldSetInProcessData = false;
	probe->onInitialized = [&](TestConnection& con) {
		fieldSetInInitialized = lsCon.get() == &con;
		con.sendPacket(makePacket({'g', 's'}));
	};
	probe->onPacket = [&](TestConnection& con, const std::vector<uint8_t>& payload) {
		fieldSetInProcessData = lsCon.get() == &con;
		std::lock_guard lock(probe->mutex);
		probe->received.push_back(payload);
		return true;
	};
	asio::ip::tcp::socket socket = gameServer.openSocket({"127.0.0.1", loginServer.port});
	lsCon = std::make_shared<TestConnection>(std::move(socket), gameServer, probe);
	lsCon->sendPacket(makePacket({'e', 'a', 'r', 'l', 'y'})); // queued, sent once IO starts
	EXPECT_EQ(probe->initializedCount, 0);
	gameServer.registerConnection(lsCon);

	EXPECT_EQ(probe->initializedCount, 1);
	EXPECT_TRUE(fieldSetInInitialized);
	EXPECT_EQ(lsCon->getIP(), "127.0.0.1");
	EXPECT_EQ(gameServer.getActiveConnectionCount(), 1u);
	ASSERT_TRUE(waitUntil([&] { return probe->receivedCount() == 1 && loginServer.probe->receivedCount() == 2; }));
	EXPECT_TRUE(fieldSetInProcessData);
	EXPECT_EQ(probe->receivedCopy()[0], std::vector<uint8_t>({'l', 's'}));
	EXPECT_EQ(loginServer.probe->receivedCopy()[0], std::vector<uint8_t>({'e', 'a', 'r', 'l', 'y'}));
	EXPECT_EQ(loginServer.probe->receivedCopy()[1], std::vector<uint8_t>({'g', 's'}));
	EXPECT_THROW(gameServer.registerConnection(lsCon), utils::IllegalStateException); // already registered

	// the remote side closes: the outbound connection is disconnected like an accepted one
	loginServer.connection(0)->close();
	ASSERT_TRUE(waitUntil([&] { return probe->disconnectCount == 1; }));
	EXPECT_EQ(gameServer.getActiveConnectionCount(), 0u);
}

TEST(NioServerTest, OpenSocketAcceptsHostNames) {
	TestServer loginServer;
	loginServer.start();
	auto probe = std::make_shared<Probe>();
	NioServer gameServer(1, {});
	gameServer.connect();
	auto connection = std::make_shared<TestConnection>(gameServer.openSocket({"localhost", loginServer.port}), gameServer, probe);
	gameServer.registerConnection(connection);
	EXPECT_EQ(connection->getIP(), "127.0.0.1"); // IPv4 is preferred
	ASSERT_TRUE(waitUntil([&] { return loginServer.server->getActiveConnectionCount() == 1; }));
}

TEST(NioServerTest, OutboundConnectionFailures) {
	auto probe = std::make_shared<Probe>();
	NioServer server(1, {});
	EXPECT_THROW(server.openSocket({"127.0.0.1", 1}), utils::IllegalStateException); // not started

	server.connect();
	EXPECT_THROW(server.openSocket({"127.0.0.1", unusedPort()}), network::SocketException);
	EXPECT_THROW(server.openSocket({"host.name.that.does.not.exist.invalid", 1234}), utils::IOException);
	EXPECT_THROW(server.registerConnection(nullptr), utils::IllegalArgumentException);

	TestServer target;
	target.start();
	// a connection of another server
	auto foreign = std::make_shared<TestConnection>(target.server->openSocket({"127.0.0.1", target.port}), *target.server, probe);
	EXPECT_THROW(server.registerConnection(foreign), utils::IllegalArgumentException);

	auto lateConnection = std::make_shared<TestConnection>(server.openSocket({"127.0.0.1", target.port}), server, probe);
	server.shutdown();
	EXPECT_THROW(server.openSocket({"127.0.0.1", target.port}), utils::IllegalStateException);
	EXPECT_THROW(server.registerConnection(lateConnection), utils::IllegalStateException);
	EXPECT_EQ(probe->initializedCount, 0);
}

TEST(NioServerTest, ShutdownClosesAllConnectionsGracefully) {
	LogCapture& logs = LogCapture::instance();
	logs.clear();
	TestServer server;
	server.probe->serverCloseAction = Probe::ServerCloseAction::CLOSE_WITH_PACKET;
	server.start(2);
	std::vector<std::unique_ptr<TestClient>> clients;
	for (int i = 0; i < 3; i++)
		clients.push_back(std::make_unique<TestClient>(server.port));
	ASSERT_TRUE(waitUntil([&] { return server.server->getActiveConnectionCount() == 3; }));

	std::thread shutdownThread([&] { server.server->shutdown(); });
	for (auto& client : clients) {
		EXPECT_EQ(client->readFrame(), std::vector<uint8_t>({'b', 'y', 'e'}));
		EXPECT_TRUE(client->waitForClose());
	}
	shutdownThread.join();

	EXPECT_EQ(server.probe->serverCloseCount, 3);
	EXPECT_EQ(server.probe->disconnectCount, 3); // shutdown waits for all onDisconnect callbacks
	EXPECT_EQ(server.server->getActiveConnectionCount(), 0u);
	EXPECT_TRUE(logs.contains("info|com.aionemu.commons.network.NioServer|Closing ServerChannels..."));
	EXPECT_TRUE(logs.contains("info|com.aionemu.commons.network.NioServer|ServerChannels closed."));
	EXPECT_TRUE(logs.contains("info|com.aionemu.commons.network.NioServer|\tClosing 3 connections..."));
	EXPECT_TRUE(logs.contains("info|com.aionemu.commons.network.NioServer|\tActive connections left: 0"));
	EXPECT_FALSE(logs.contains("Forcing"));

	// no longer accepting (checked with a short timeout, since a refused connect takes 2 seconds on Windows)
	asio::io_context io;
	asio::ip::tcp::socket socket(io);
	bool connected = false;
	socket.async_connect({asio::ip::make_address_v4("127.0.0.1"), server.port}, [&](const std::error_code& error) { connected = !error; });
	io.run_for(200ms);
	EXPECT_FALSE(connected);
	// old connections ignore further calls
	server.connection(0)->sendPacket(makePacket({1}));
	server.connection(0)->close();
	server.server->shutdown(); // second call does nothing
	EXPECT_EQ(server.probe->serverCloseCount, 3);
}

TEST(NioServerTest, ShutdownDisconnectsConnectionsIgnoringServerClose) {
	LogCapture& logs = LogCapture::instance();
	logs.clear();
	TestServer server;
	server.probe->serverCloseAction = Probe::ServerCloseAction::DO_NOTHING;
	server.start();
	TestClient client(server.port);
	ASSERT_TRUE(waitUntil([&] { return server.server->getActiveConnectionCount() == 1; }));

	auto start = std::chrono::steady_clock::now();
	server.server->shutdown();
	EXPECT_LT(std::chrono::steady_clock::now() - start, 4s); // does not wait for the 5 second close timeout
	// like Java, a connection that does not close itself is not pending close, so it is only reported ...
	EXPECT_TRUE(logs.contains("\tActive connections left: 1"));
	// ... but then disconnected before the IO threads stop
	EXPECT_EQ(server.probe->disconnectCount, 1);
	EXPECT_TRUE(client.waitForClose());
}

TEST(NioServerTest, ShutdownForcesStragglersAfterTimeout) {
	LogCapture& logs = LogCapture::instance();
	logs.clear();
	TestServer server;
	server.wbSize = 8192;
	server.probe->serverCloseAction = Probe::ServerCloseAction::CLOSE; // close() keeps the queue, which cannot be flushed
	server.start();
	TestClient stuckClient(server.port); // never reads
	TestClient normalClient(server.port);
	ASSERT_TRUE(waitUntil([&] { return server.server->getActiveConnectionCount() == 2; }));
	auto stuck = server.connection(0);
	auto packet = makePacket(std::vector<uint8_t>(8000, 1));
	for (int i = 0; i < 40000; i++)
		stuck->sendPacket(packet);

	auto start = std::chrono::steady_clock::now();
	server.server->shutdown(300ms);
	auto elapsed = std::chrono::steady_clock::now() - start;
	EXPECT_LT(elapsed, 1900ms); // below the 2 second close deadline
	EXPECT_TRUE(logs.contains("\tClosing 2 connections..."));
	EXPECT_TRUE(logs.contains("\tForcing 1 connections to disconnect..."));
	EXPECT_TRUE(logs.contains("\tActive connections left: 1"));
	EXPECT_EQ(server.probe->disconnectCount, 2);
	EXPECT_TRUE(stuck->isClosed());
	EXPECT_TRUE(normalClient.waitForClose());
}

TEST(NioServerTest, ShutdownBeforeConnectAndDestructionAreSafe) {
	{
		NioServer server(2, {});
		server.shutdown();
		EXPECT_THROW(server.connect(), utils::IllegalStateException);
	}
	{
		NioServer neverStarted(2, {});
	}

	// connections may outlive their server
	std::shared_ptr<TestConnection> survivor;
	std::shared_ptr<Probe> probe;
	{
		TestServer server;
		server.start();
		probe = server.probe;
		TestClient client(server.port);
		survivor = server.connection(0);
		ASSERT_TRUE(survivor);
		server.server.reset(); // destructor shuts down
		EXPECT_TRUE(survivor->isClosed());
		EXPECT_EQ(probe->disconnectCount, 1);
	}
	survivor->sendPacket(makePacket({1}));
	survivor->close();
	survivor.reset();
}

TEST(NioServerTest, ShutdownFromIoThreadIsRejected) {
	TestServer server;
	std::atomic<bool> threw = false;
	server.start();
	TestServer* serverPtr = &server;
	server.probe->onPacket = [&](TestConnection&, const std::vector<uint8_t>&) {
		try {
			serverPtr->server->shutdown();
		} catch (const utils::IllegalStateException&) {
			threw = true;
		}
		return true;
	};
	TestClient client(server.port);
	client.send(frame({1}));
	ASSERT_TRUE(waitUntil([&] { return threw.load(); }));
}

TEST(NioServerTest, ConnectionAcceptedWhileShutdownBeginsIsInitializedAndDisconnected) {
	LogCapture& logs = LogCapture::instance();
	logs.clear();
	auto probe = std::make_shared<Probe>();
	std::atomic<bool> factoryEntered = false;
	ServerCfg cfg{{"127.0.0.1", 0}, "test clients", [&](asio::ip::tcp::socket socket, NioServer& nioServer) -> std::shared_ptr<network::AConnectionBase> {
		factoryEntered = true;
		// let shutdown begin while the factory is running
		waitUntil([&] { return logs.contains("Closing ServerChannels..."); });
		std::this_thread::sleep_for(100ms);
		return std::make_shared<TestConnection>(std::move(socket), nioServer, probe);
	}};
	NioServer server(2, {cfg});
	server.connect();
	TestClient client(server.getBoundAddresses().at(0).port);
	ASSERT_TRUE(waitUntil([&] { return factoryEntered.load(); }));
	server.shutdown();

	// like Java's Acceptor, a connection created by the factory is always registered and initialized, so it takes part in the shutdown
	EXPECT_EQ(probe->initializedCount, 1);
	EXPECT_EQ(probe->serverCloseCount, 1);
	EXPECT_EQ(probe->disconnectCount, 1);
	EXPECT_EQ(server.getActiveConnectionCount(), 0u);
	EXPECT_TRUE(client.waitForClose());
}

TEST(NioServerTest, AcceptingContinuesAfterFactoryException) {
	LogCapture& logs = LogCapture::instance();
	logs.clear();
	TestServer server;
	server.factoryThrows = 2;
	server.start(1);
	TestClient failed1(server.port);
	EXPECT_TRUE(failed1.waitForClose());
	TestClient failed2(server.port);
	EXPECT_TRUE(failed2.waitForClose());
	EXPECT_TRUE(waitUntil([&] { return logs.count("factory failure") == 2; })); // logged after the socket was closed

	TestClient client(server.port);
	client.send(frame({9}));
	ASSERT_TRUE(waitUntil([&] { return server.probe->receivedCount() == 1; }));
	EXPECT_EQ(server.probe->initializedCount, 1);
	EXPECT_EQ(server.server->getActiveConnectionCount(), 1u);
}

TEST(NioServerTest, AcceptingContinuesAfterRegistrationAndInitializationErrors) {
	LogCapture& logs = LogCapture::instance();
	logs.clear();
	auto probe = std::make_shared<Probe>();
	std::atomic<int> factoryCalls = 0;
	std::shared_ptr<TestConnection> first;
	ServerCfg cfg{{"127.0.0.1", 0}, "test clients", [&](asio::ip::tcp::socket socket, NioServer& nioServer) -> std::shared_ptr<network::AConnectionBase> {
		int call = ++factoryCalls;
		if (call == 2)
			return first; // already registered: registering it again throws after the factory returned
		auto connection = std::make_shared<TestConnection>(std::move(socket), nioServer, probe);
		if (call == 1)
			first = connection;
		return connection;
	}};
	probe->onInitialized = [&](TestConnection&) {
		if (probe->initializedCount == 2)
			throw std::runtime_error("initialization failure");
	};
	NioServer server(0, {cfg});
	server.connect();
	uint16_t port = server.getBoundAddresses().at(0).port;

	TestClient client1(port);
	ASSERT_TRUE(waitUntil([&] { return server.getActiveConnectionCount() == 1; }));
	TestClient client2(port);
	EXPECT_TRUE(client2.waitForClose());
	EXPECT_TRUE(waitUntil([&] { return logs.contains("TestConnection 127.0.0.1 is already registered"); }));
	TestClient client3(port);
	EXPECT_TRUE(client3.waitForClose()); // an exception in initialized() closes the connection
	ASSERT_TRUE(waitUntil([&] { return probe->disconnectCount == 1; }));
	EXPECT_TRUE(logs.contains("Error initializing TestConnection 127.0.0.1"));

	TestClient client4(port);
	client4.send(frame({4}));
	ASSERT_TRUE(waitUntil([&] { return probe->receivedCount() == 1; }));
	EXPECT_EQ(probe->initializedCount, 3);
	EXPECT_EQ(server.getActiveConnectionCount(), 2u);
	EXPECT_EQ(factoryCalls, 4);
}

TEST(NioServerTest, SeveralListenersOnOneServer) {
	auto probe = std::make_shared<Probe>();
	std::mutex mutex;
	std::vector<std::string> descriptions;
	auto factoryFor = [&](std::string description) {
		return [&, description](asio::ip::tcp::socket socket, NioServer& nioServer) -> std::shared_ptr<network::AConnectionBase> {
			{
				std::lock_guard lock(mutex);
				descriptions.push_back(description);
			}
			return std::make_shared<TestConnection>(std::move(socket), nioServer, probe);
		};
	};
	NioServer server(2, {ServerCfg{{"127.0.0.1", 0}, "clients", factoryFor("clients")},
												ServerCfg{{"127.0.0.1", 0}, "game servers", factoryFor("game servers")}});
	server.connect();
	auto addresses = server.getBoundAddresses();
	ASSERT_EQ(addresses.size(), 2u);
	EXPECT_NE(addresses[0].port, addresses[1].port);

	TestClient gameServer(addresses[1].port);
	ASSERT_TRUE(waitUntil([&] { return server.getActiveConnectionCount() == 1; }));
	TestClient client(addresses[0].port);
	ASSERT_TRUE(waitUntil([&] { return server.getActiveConnectionCount() == 2; }));
	{
		std::lock_guard lock(mutex);
		EXPECT_EQ(descriptions, (std::vector<std::string>{"game servers", "clients"}));
	}
	server.shutdown();
	EXPECT_EQ(probe->disconnectCount, 2);
	EXPECT_TRUE(client.waitForClose());
	EXPECT_TRUE(gameServer.waitForClose());
}

TEST(NioServerTest, DisconnectExecutorRunsOnDisconnectCallbacks) {
	std::mutex mutex;
	std::vector<std::thread> executorThreads;
	std::atomic<int> executed = 0;
	TestServer server;
	server.dcExecutor = [&](std::function<void()> task) {
		executed++;
		std::lock_guard lock(mutex);
		executorThreads.emplace_back([task = std::move(task)] {
			utils::concurrent::setCurrentThreadName("TestExecutor");
			std::this_thread::sleep_for(100ms);
			task();
		});
	};
	server.start();
	{
		TestClient client(server.port);
		ASSERT_TRUE(waitUntil([&] { return server.server->getActiveConnectionCount() == 1; }));
		client.close();
		ASSERT_TRUE(waitUntil([&] { return server.probe->disconnectCount == 1; }));
	}
	EXPECT_EQ(executed, 1);
	EXPECT_FALSE(server.probe->onDisconnectOnIoThread);

	TestClient client(server.port);
	ASSERT_TRUE(waitUntil([&] { return server.server->getActiveConnectionCount() == 1; }));
	server.server->shutdown();
	EXPECT_EQ(executed, 2);
	EXPECT_EQ(server.probe->disconnectCount, 2); // shutdown waited for the delayed callback
	std::lock_guard lock(mutex);
	for (auto& thread : executorThreads)
		thread.join();
}

TEST(NioServerTest, RejectingDisconnectExecutorStillCallsOnDisconnect) {
	LogCapture& logs = LogCapture::instance();
	logs.clear();
	TestServer server;
	server.dcExecutor = [](std::function<void()>) { throw std::runtime_error("rejected"); };
	server.start();
	TestClient client(server.port);
	ASSERT_TRUE(waitUntil([&] { return server.server->getActiveConnectionCount() == 1; }));
	client.close();
	ASSERT_TRUE(waitUntil([&] { return server.probe->disconnectCount == 1; }));
	EXPECT_EQ(server.server->getActiveConnectionCount(), 0u);
	EXPECT_TRUE(logs.contains("Could not dispatch onDisconnect, calling it on the current thread: TestConnection 127.0.0.1"));

	NioServer notStarted(1, {});
	EXPECT_THROW(notStarted.connect(NioServer::DisconnectExecutor()), utils::IllegalArgumentException);
	notStarted.connect([](std::function<void()> task) { task(); }); // the invalid call did not start the server
}
