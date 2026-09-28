#include "aion/commons/network/NioServer.h"

#include <algorithm>
#include <future>
#include <string>

#include <asio/bind_executor.hpp>
#include <asio/post.hpp>
#include <asio/steady_timer.hpp>
#include <asio/strand.hpp>
#include <fmt/format.h>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/network/SocketException.h"
#include "aion/commons/network/detail/ServerContext.h"
#include "aion/commons/utils/concurrent/ThreadName.h"

namespace aion::commons::network {

namespace {

// Intentionally leaked and created on first use, so the network classes can be constructed and destroyed as statics (Java: static fields) without
// depending on the initialization or destruction order of namespace scope statics.
const logging::Logger& log() {
	static const auto* logger = new logging::Logger(logging::LoggerFactory::getLogger("com.aionemu.commons.network.NioServer"));
	return *logger;
}

/** Java: Thread.sleep(100) in the shutdown wait loop */
constexpr auto SHUTDOWN_POLL_INTERVAL = std::chrono::milliseconds(100);
/** delay before accepting again after an accept error (e.g. too many open files), so the IO thread does not spin */
constexpr auto ACCEPT_RETRY_DELAY = std::chrono::milliseconds(100);

/**
 * Resolves the address to one endpoint. IPv4 addresses are preferred when a host name resolves to several addresses, like Java does by default
 * (java.net.preferIPv6Addresses=false).
 */
asio::ip::tcp::endpoint resolveEndpoint(asio::io_context& ioContext, const utils::InetSocketAddress& address) {
	if (address.host.empty() || address.host == "0.0.0.0")
		return {asio::ip::tcp::v4(), address.port};
	if (address.host == "::")
		return {asio::ip::tcp::v6(), address.port};

	asio::error_code error;
	asio::ip::address ip = asio::ip::make_address(address.host, error);
	if (!error)
		return {ip, address.port};

	asio::ip::tcp::resolver resolver(ioContext);
	auto results = resolver.resolve(address.host, std::to_string(address.port), error);
	if (error || results.empty())
		throw utils::IOException(fmt::format("Could not resolve {}: {}", address.host, error ? error.message() : "no address found"));
	for (const auto& entry : results) {
		if (entry.endpoint().address().is_v4())
			return entry.endpoint();
	}
	return results.begin()->endpoint();
}

std::string describe(const AConnectionBase& connection) noexcept {
	try {
		return connection.toString();
	} catch (...) {
		return connection.getIP();
	}
}

/** Logs the exception currently being handled; never throws (for handlers and noexcept paths). */
void logCurrentException(const char* message) noexcept {
	try {
		log().errorCurrentException(message);
	} catch (...) {
	}
}

/** Runs the io_context until it is stopped or out of work, logging exceptions that escape handlers (Java: Dispatcher.run). */
void runContext(asio::io_context& ioContext) {
	for (;;) {
		try {
			ioContext.run();
			return;
		} catch (...) {
			try {
				log().errorCurrentException("");
			} catch (...) {
			}
		}
	}
}

} // namespace

/** One listening socket (Java: Acceptor + the server channel's SelectionKey). All acceptor operations run on its strand. */
struct NioServer::Listener {
	Listener(asio::io_context& ioContext, const ServerCfg& cfg) : cfg(cfg), strand(asio::make_strand(ioContext)), acceptor(strand), retryTimer(strand) {}

	const ServerCfg& cfg;
	asio::strand<asio::io_context::executor_type> strand;
	asio::ip::tcp::acceptor acceptor;
	asio::steady_timer retryTimer;
	bool closed = false;
};

NioServer::NioServer(int32_t readWriteThreads, std::vector<ServerCfg> cfgs, int32_t disconnectThreads)
	: context(std::make_shared<detail::ServerContext>()), readWriteThreads(readWriteThreads), disconnectThreadCount(std::max(1, disconnectThreads)),
		cfgs(std::move(cfgs)) {
}


NioServer::~NioServer() {
	try {
		if (isOwnThread()) {
			log().error("NioServer destroyed from one of its own threads, its threads are detached");
			for (auto& thread : ioThreads)
				thread.detach();
			for (auto& thread : disconnectThreads)
				thread.detach();
			return;
		}
		shutdown();
	} catch (...) {
		try {
			log().errorCurrentException("Error shutting down NioServer");
		} catch (...) {
		}
	}
}

void NioServer::connect(DisconnectExecutor dcExecutor) {
	if (!dcExecutor)
		throw utils::IllegalArgumentException("dcExecutor must not be empty");
	start(std::move(dcExecutor));
}

void NioServer::connect() {
	start({});
}

void NioServer::start(DisconnectExecutor dcExecutor) {
	std::lock_guard lock(stateMutex);
	if (state != State::NEW)
		throw utils::IllegalStateException("NioServer was already started");

	std::vector<std::shared_ptr<Listener>> newListeners;
	try {
		for (const ServerCfg& cfg : cfgs) {
			auto listener = std::make_shared<Listener>(context->ioContext, cfg);
			asio::ip::tcp::endpoint endpoint = resolveEndpoint(context->ioContext, cfg.address);
			listener->acceptor.open(endpoint.protocol());
#ifdef _WIN32
			// like Java on Windows (sun.net.useExclusiveBind): no other socket may bind the same port. SO_REUSEADDR is not set, since on Windows it
			// would allow binding a port that is already in use.
			listener->acceptor.set_option(asio::detail::socket_option::boolean<SOL_SOCKET, SO_EXCLUSIVEADDRUSE>(true));
#else
			// Java sets SO_REUSEADDR on server sockets on POSIX systems, so a restarted server can bind while old connections are in TIME_WAIT
			listener->acceptor.set_option(asio::ip::tcp::acceptor::reuse_address(true));
#endif
			listener->acceptor.bind(endpoint);
			listener->acceptor.listen(); // backlog SOMAXCONN (Java: 50)
			log().info("Listening on " + cfg.getAddressInfo() + " for " + cfg.clientDescription);
			newListeners.push_back(std::move(listener));
		}
	} catch (const std::exception& e) {
		throw utils::IOException(std::string("Could not open server socket: ") + e.what(), std::current_exception());
	}

	listeners = std::move(newListeners);
	// set before any thread starts, afterwards it is only read
	const int32_t ownDisconnectThreads = dcExecutor ? 0 : disconnectThreadCount;
	context->dcExecutor = std::move(dcExecutor);
	ioWork.emplace(context->ioContext.get_executor());
	if (ownDisconnectThreads > 0)
		disconnectWork.emplace(context->disconnectContext.get_executor());
	int32_t ioThreadCount = std::max(1, readWriteThreads);
	try {
		for (int32_t i = 0; i < ioThreadCount; i++) {
			std::string name = readWriteThreads < 1 ? "AcceptReadWrite Dispatcher" : fmt::format("ReadWrite-{} Dispatcher", i);
			{
				std::lock_guard contextLock(context->mutex);
				context->runningIoThreads++;
			}
			ioThreads.emplace_back([ctx = context, name = std::move(name)] {
				utils::concurrent::setCurrentThreadName(name);
				runContext(ctx->ioContext);
				{
					std::lock_guard contextLock(ctx->mutex);
					ctx->runningIoThreads--;
				}
				ctx->stateChanged.notify_all();
			});
		}
		for (int32_t i = 0; i < ownDisconnectThreads; i++) {
			disconnectThreads.emplace_back([ctx = context, name = fmt::format("DisconnectExecutor-{}", i)] {
				utils::concurrent::setCurrentThreadName(name);
				runContext(ctx->disconnectContext);
			});
		}
	} catch (...) {
		// thread creation failed: undo everything so the server is not left half started
		context->ioContext.stop();
		context->disconnectContext.stop();
		for (auto& thread : ioThreads)
			thread.join();
		for (auto& thread : disconnectThreads)
			thread.join();
		throw;
	}

	for (const auto& listener : listeners)
		asio::post(listener->strand, [this, listener] { startAccept(listener); });
	state = State::RUNNING;
}

void NioServer::startAccept(const std::shared_ptr<Listener>& listener) {
	if (listener->closed)
		return;
	listener->acceptor.async_accept(context->ioContext,
		asio::bind_executor(listener->strand, [this, listener](const std::error_code& error, asio::basic_stream_socket<asio::ip::tcp, asio::io_context::executor_type> accepted) {
			onAccept(listener, error, asio::ip::tcp::socket(std::move(accepted)));
		}));
}

void NioServer::onAccept(const std::shared_ptr<Listener>& listener, const std::error_code& error, asio::ip::tcp::socket socket) {
	// closeListeners runs on the listener's strand, so a connection accepted here is registered before shutdown looks for active connections
	if (listener->closed)
		return;
	bool retry = false;
	try {
		if (error) {
			if (error == asio::error::operation_aborted)
				return;
			retry = true; // e.g. too many open files
			log().error(fmt::format("Error accepting connection on {} for {}: {}", listener->cfg.getAddressInfo(), listener->cfg.clientDescription, error.message()));
		} else {
			acceptConnection(*listener, std::move(socket));
		}
	} catch (...) {
		logCurrentException("");
	}
	// accepting must go on whatever happened above (Java: the server channel stays registered with the selector)
	if (!retry) {
		try {
			startAccept(listener);
			return;
		} catch (...) {
			logCurrentException("Could not accept connections, trying again later");
		}
	}
	scheduleAcceptRetry(listener);
}

void NioServer::scheduleAcceptRetry(const std::shared_ptr<Listener>& listener) {
	try {
		listener->retryTimer.expires_after(ACCEPT_RETRY_DELAY);
		listener->retryTimer.async_wait(asio::bind_executor(listener->strand, [this, listener](const std::error_code& timerError) {
			if (timerError || listener->closed)
				return;
			try {
				startAccept(listener);
			} catch (...) {
				logCurrentException("Could not accept connections, trying again later");
				scheduleAcceptRetry(listener);
			}
		}));
	} catch (...) {
		logCurrentException("Could not schedule accepting connections, no more connections will be accepted");
	}
}

void NioServer::acceptConnection(const Listener& listener, asio::ip::tcp::socket socket) {
	asio::error_code ignored;
	socket.set_option(asio::ip::tcp::no_delay(true), ignored);
	// Deviation: Java sets SO_LINGER(true, 10). Here sockets are closed gracefully after all queued data was handed to the socket, and the OS
	// delivers the remaining bytes in the background, which avoids blocking an IO thread in close().

	std::shared_ptr<AConnectionBase> connection;
	try {
		if (listener.cfg.connectionFactory)
			connection = listener.cfg.connectionFactory(std::move(socket), *this);
		else
			log().error("No connection factory configured for " + listener.cfg.clientDescription);
	} catch (...) {
		logCurrentException("");
	}
	// a null connection means the factory rejected the socket, which is closed by its destructor
	if (!connection)
		return;
	// Java: a created connection is always registered and initialized. Here that fails only if shutdown timed out waiting for this listener.
	if (addConnection(connection, false))
		initializeAndStartIo(connection);
	else
		log().warn("Dropped " + describe(*connection) + ": the server is shutting down");
}

asio::ip::tcp::socket NioServer::openSocket(const utils::InetSocketAddress& address) {
	{
		std::lock_guard lock(stateMutex);
		if (state != State::RUNNING)
			throw utils::IllegalStateException("Cannot connect to " + address.toString() + ": NioServer is not running");
	}
	asio::ip::tcp::endpoint endpoint = resolveEndpoint(context->ioContext, address);
	asio::ip::tcp::socket socket(context->ioContext);
	asio::error_code error;
	socket.connect(endpoint, error);
	if (error)
		throw SocketException(fmt::format("Could not connect to {}: {}", address.toString(), error.message()));
	socket.set_option(asio::ip::tcp::no_delay(true), error);
	return socket;
}

void NioServer::registerConnection(const std::shared_ptr<AConnectionBase>& connection) {
	if (!connection)
		throw utils::IllegalArgumentException("Cannot register a null connection");
	addConnection(connection, true);
	initializeAndStartIo(connection);
}

bool NioServer::addConnection(const std::shared_ptr<AConnectionBase>& connection, bool outbound) {
	if (connection->context != context)
		throw utils::IllegalArgumentException("The connection was created for another NioServer");
	std::lock_guard lock(context->mutex);
	if (!context->acceptingConnections) {
		if (outbound)
			throw utils::IllegalStateException("Cannot register " + describe(*connection) + ": NioServer is not running");
		return false;
	}
	if (connection->registered.exchange(true))
		throw utils::IllegalStateException(describe(*connection) + " is already registered");
	context->activeConnections.insert(connection);
	return true;
}

void NioServer::initializeAndStartIo(const std::shared_ptr<AConnectionBase>& connection) {
	try {
		connection->initialized();
	} catch (...) {
		// Deviation: in Java the exception escapes to the accept dispatcher (or the caller of an outbound connect) and the connection stays open
		try {
			log().errorCurrentException("Error initializing " + describe(*connection));
		} catch (...) {
		}
		connection->close();
	}
	try {
		connection->startIo();
	} catch (...) {
		// nothing was posted for the connection yet (see AConnectionBase::startIo), so it can be disconnected from this thread
		logCurrentException("Could not start IO, disconnecting");
		connection->disconnect();
	}
}

void NioServer::shutdown(std::chrono::milliseconds closeTimeout) {
	{
		std::lock_guard lock(stateMutex);
		if (state == State::NEW) {
			state = State::SHUT_DOWN;
			return;
		}
		if (state != State::RUNNING)
			return;
		if (isOwnThread())
			throw utils::IllegalStateException("NioServer::shutdown must not be called from its IO or disconnect threads");
		state = State::SHUTTING_DOWN;
	}

	log().info("Closing ServerChannels...");
	closeListeners();
	{
		// after closeListeners, so a connection created by an accept that was already in progress is still registered (see onAccept)
		std::lock_guard lock(context->mutex);
		context->acceptingConnections = false;
	}
	log().info("ServerChannels closed.");

	// find active connections once, at this point new ones cannot be added anymore
	std::vector<std::shared_ptr<AConnectionBase>> activeConnections;
	{
		std::lock_guard lock(context->mutex);
		activeConnections.assign(context->activeConnections.begin(), context->activeConnections.end());
	}
	auto isClosed = [](const std::shared_ptr<AConnectionBase>& connection) { return connection->isClosed(); };
	if (!activeConnections.empty()) {
		log().info("\tClosing " + std::to_string(activeConnections.size()) + " connections...");

		// notify connections about server close (they should close themselves)
		for (const auto& connection : activeConnections) {
			try {
				connection->onServerClose();
			} catch (...) {
				log().errorCurrentException("Error in onServerClose of " + describe(*connection));
			}
		}

		// wait for connections to close or force close them
		auto timeout = std::chrono::steady_clock::now() + closeTimeout;
		auto isAnyConnectionClosePending = [&] {
			return std::ranges::any_of(activeConnections, [](const auto& connection) { return connection->isPendingClose(); });
		};
		while (isAnyConnectionClosePending()) {
			{
				std::unique_lock lock(context->mutex);
				context->stateChanged.wait_for(lock, SHUTDOWN_POLL_INTERVAL);
			}
			if (std::chrono::steady_clock::now() > timeout) {
				std::erase_if(activeConnections, isClosed);
				log().info("\tForcing " + std::to_string(activeConnections.size()) + " connections to disconnect...");
				for (const auto& connection : activeConnections)
					connection->close();
				break;
			}
		}
		std::erase_if(activeConnections, isClosed);
		log().info("\tActive connections left: " + std::to_string(activeConnections.size()));
	}
	activeConnections.clear();

	// Deviation: Java keeps its dispatcher threads running. Here the IO threads are stopped, so all connections are disconnected first, which
	// guarantees that onDisconnect is called for each of them.
	{
		std::unique_lock lock(context->mutex);
		for (const auto& connection : context->activeConnections)
			connection->forceDisconnect();
		if (!context->stateChanged.wait_for(lock, closeTimeout, [&] { return context->activeConnections.empty(); }))
			log().warn(fmt::format("{} connections could not be disconnected in time", context->activeConnections.size()));
	}

	stopThreads(closeTimeout);

	std::lock_guard lock(stateMutex);
	state = State::SHUT_DOWN;
}

void NioServer::closeListeners() {
	for (const auto& listener : listeners) {
		auto done = std::make_shared<std::promise<void>>();
		std::future<void> closed = done->get_future();
		asio::post(listener->strand, [listener, done] {
			listener->closed = true;
			asio::error_code ignored;
			listener->acceptor.close(ignored);
			listener->retryTimer.cancel();
			done->set_value();
		});
		if (closed.wait_for(std::chrono::seconds(5)) != std::future_status::ready)
			log().warn("Timeout closing server socket for " + listener->cfg.clientDescription);
	}
}

void NioServer::stopThreads(std::chrono::milliseconds timeout) {
	ioWork.reset();
	{
		std::unique_lock lock(context->mutex);
		// without the work guard, the IO threads return as soon as all pending operations completed
		if (!context->stateChanged.wait_for(lock, timeout, [&] { return context->runningIoThreads == 0; })) {
			log().warn("IO threads did not finish in time, stopping them");
			context->ioContext.stop();
		}
	}
	for (auto& thread : ioThreads)
		thread.join();
	ioThreads.clear();

	// wait for all pending onDisconnect callbacks
	waitForDisconnectCallbacks(timeout);
	disconnectWork.reset();
	for (auto& thread : disconnectThreads)
		thread.join();
	disconnectThreads.clear();
}

void NioServer::waitForDisconnectCallbacks(std::chrono::milliseconds timeout) {
	if (!context->dcExecutor)
		return; // joining the own disconnect threads waits for all callbacks
	std::unique_lock lock(context->mutex);
	if (!context->stateChanged.wait_for(lock, timeout, [&] { return context->pendingDisconnects == 0; }))
		log().warn(fmt::format("{} onDisconnect callbacks did not finish in time", context->pendingDisconnects));
}

bool NioServer::isOwnThread() const {
	auto id = std::this_thread::get_id();
	auto matches = [id](const std::thread& thread) { return thread.get_id() == id; };
	return std::ranges::any_of(ioThreads, matches) || std::ranges::any_of(disconnectThreads, matches);
}

std::vector<utils::InetSocketAddress> NioServer::getBoundAddresses() const {
	std::lock_guard lock(stateMutex);
	std::vector<utils::InetSocketAddress> addresses;
	for (const auto& listener : listeners) {
		asio::error_code error;
		asio::ip::tcp::endpoint endpoint = listener->acceptor.local_endpoint(error);
		addresses.push_back({error ? std::string() : endpoint.address().to_string(), error ? uint16_t(0) : endpoint.port()});
	}
	return addresses;
}

size_t NioServer::getActiveConnectionCount() const {
	std::lock_guard lock(context->mutex);
	return context->activeConnections.size();
}

} // namespace aion::commons::network
