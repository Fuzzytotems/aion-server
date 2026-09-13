#pragma once

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>
#include <utility>
#include <vector>

#include <asio/executor_work_guard.hpp>
#include <asio/io_context.hpp>
#include <asio/ip/tcp.hpp>

#include "aion/commons/network/AConnection.h"
#include "aion/commons/network/ServerCfg.h"
#include "aion/commons/utils/Exception.h"
#include "aion/commons/utils/InetSocketAddress.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::commons::network {

namespace detail {
struct ServerContext;
}

/**
 * Network server handling connections on the configured addresses, plus outbound connections to other servers.
 * <p>
 * <b>Mapping of the Java NIO implementation to Asio:</b>
 * <table>
 * <tr><th>Java</th><th>C++</th></tr>
 * <tr><td>Dispatcher threads with a Selector each</td><td>one asio::io_context run by several IO threads</td></tr>
 * <tr><td>AcceptDispatcherImpl + Acceptor</td><td>one asio acceptor per ServerCfg with an async accept loop on the IO threads</td></tr>
 * <tr><td>AcceptReadWriteDispatcherImpl (read/write/pending close)</td><td>per connection async read/write loops on a strand, and a close
 * deadline timer (see AConnectionBase)</td></tr>
 * <tr><td>Dispatcher.register(channel, OP_READ, con) + con.initialized()</td><td>done by the server for accepted connections, registerConnection for
 * outbound ones</td></tr>
 * <tr><td>dcExecutor passed to connect()</td><td>the DisconnectExecutor passed to connect(dcExecutor), or a disconnect thread pool owned by the server
 * (connect())</td></tr>
 * </table>
 * <b>Threads:</b> Java creates readWriteThreads read/write dispatchers plus one accept dispatcher, or a single dispatcher doing everything if
 * readWriteThreads is 0. Here max(1, readWriteThreads) IO threads do accepting and IO alike, so 0 and 1 both mean that all network code
 * (accepting, the ConnectionFactory, initialized() of accepted connections, processData and writeData) runs on one thread, which the game server
 * relies on. IO threads are named "AcceptReadWrite Dispatcher" (0 threads configured) or "ReadWrite-&lt;n&gt; Dispatcher", disconnect threads
 * "DisconnectExecutor-&lt;n&gt;".
 * <p>
 * Java: com.aionemu.commons.network.NioServer, Dispatcher, AcceptDispatcherImpl, AcceptReadWriteDispatcherImpl, Acceptor
 *
 * @author -Nemesiss-
 */
class NioServer {
public:
	/**
	 * Executes onDisconnect callbacks (Java: the Executor passed to connect(), e.g. ThreadPoolManager or a cached thread pool). It is called on IO
	 * threads and must run the task on another thread; it must not block. If it throws, the task is run on the calling thread.
	 */
	using DisconnectExecutor = std::function<void(std::function<void()> task)>;

	/**
	 * @param readWriteThreads
	 *          number of IO threads, values below 1 mean 1 (see the class comment)
	 * @param cfgs
	 *          addresses to listen on, may be empty for a server that only opens outbound connections
	 * @param disconnectThreads
	 *          number of threads running onDisconnect callbacks if the server is started with connect() (at least 1), unused with
	 *          connect(dcExecutor)
	 */
	NioServer(int32_t readWriteThreads, std::vector<ServerCfg> cfgs, int32_t disconnectThreads = 2);

	/** Shuts the server down (see shutdown()) if it is still running. */
	~NioServer();

	NioServer(const NioServer&) = delete;
	NioServer& operator=(const NioServer&) = delete;

	/**
	 * Binds all listening sockets, logging "Listening on &lt;address info&gt; for &lt;client description&gt;" for each, and starts the IO threads.
	 * onDisconnect callbacks run on dcExecutor (Java: connect(Executor dcExecutor), e.g. ThreadPoolManager, a cached or a single thread pool).
	 *
	 * @throws utils::IOException
	 *           if a socket cannot be bound ("Could not open server socket: ..."); no thread is started then
	 * @throws utils::IllegalStateException
	 *           if called twice or after shutdown
	 * @throws utils::IllegalArgumentException
	 *           if dcExecutor is empty
	 */
	void connect(DisconnectExecutor dcExecutor);

	/** Like connect(dcExecutor), but onDisconnect callbacks run on disconnectThreads threads owned by this server. */
	void connect();

	/**
	 * Opens a socket connected to address (Java: SocketChannel.open(address)), synchronously. Used for outbound connections (game server -&gt;
	 * login/chat server), which are created and registered by the caller in the same order as in Java:
	 * <pre>
	 * asio::ip::tcp::socket socket = nioServer.openSocket(NetworkConfig::LOGIN_ADDRESS); // Java: SocketChannel.open(...)
	 * lsCon = std::make_shared&lt;LoginServerConnection&gt;(std::move(socket), nioServer); // set before anything can happen on the connection
	 * nioServer.registerConnection(lsCon); // Java: d.register(sc, OP_READ, lsCon); lsCon.initialized();
	 * </pre>
	 *
	 * @throws SocketException
	 *           if the connection cannot be established (e.g. refused)
	 * @throws utils::IOException
	 *           if the address cannot be resolved
	 * @throws utils::IllegalStateException
	 *           if the server is not running (connect() not called yet, or shut down)
	 */
	asio::ip::tcp::socket openSocket(const utils::InetSocketAddress& address);

	/**
	 * Registers an outbound connection created with a socket from openSocket (Java: Dispatcher.register(sc, OP_READ, con) followed by
	 * con.initialized()): the connection is added to the active connections, initialized() is called on the calling thread, and then reading and
	 * writing start. Packets sent and close() calls before this call take effect once IO starts. Nothing happens on the connection (no
	 * processData, no disconnect) before initialized() returned.
	 *
	 * @throws utils::IllegalStateException
	 *           if the server is not running or shutting down, or if the connection is already registered
	 * @throws utils::IllegalArgumentException
	 *           if connection is null or belongs to another NioServer
	 */
	void registerConnection(const std::shared_ptr<AConnectionBase>& connection);

	/**
	 * Shuts the server down with the same sequence and log messages as Java:
	 * <ol>
	 * <li>stops accepting connections ("Closing ServerChannels...", "ServerChannels closed.")</li>
	 * <li>calls onServerClose() on every active connection ("\tClosing N connections...")</li>
	 * <li>waits up to closeTimeout for connections pending close, then calls close() on the ones still open ("\tForcing N connections to
	 * disconnect...")</li>
	 * <li>logs "\tActive connections left: N"</li>
	 * </ol>
	 * Then (unlike Java, whose dispatcher threads keep running until the JVM exits) all connections that are still not disconnected are
	 * disconnected immediately, the IO threads are stopped and joined, and the call waits until all pending onDisconnect callbacks have run (with a
	 * DisconnectExecutor at most closeTimeout).
	 * Afterwards sendPacket/close on old connections do nothing and openSocket and registerConnection throw.
	 * <p>
	 * Must not be called from an IO or disconnect thread. Calling it again, or before connect(), does nothing.
	 *
	 * @param closeTimeout
	 *          how long to wait for closing connections (Java: 5 seconds)
	 */
	void shutdown(std::chrono::milliseconds closeTimeout = std::chrono::seconds(5));

	/** @return the addresses the listening sockets are actually bound to (useful when binding port 0), in ServerCfg order */
	std::vector<utils::InetSocketAddress> getBoundAddresses() const;

	/** @return number of registered connections that are not disconnected yet */
	size_t getActiveConnectionCount() const;

private:
	friend class AConnectionBase;
	struct Listener;
	enum class State { NEW, RUNNING, SHUTTING_DOWN, SHUT_DOWN };

	/** @param dcExecutor the executor for onDisconnect callbacks, or empty to use the own disconnect threads */
	void start(DisconnectExecutor dcExecutor);
	/** @return false if the connection was not registered because the server does not accept connections anymore (only if outbound is false) */
	bool addConnection(const std::shared_ptr<AConnectionBase>& connection, bool outbound);
	void initializeAndStartIo(const std::shared_ptr<AConnectionBase>& connection);
	void startAccept(const std::shared_ptr<Listener>& listener);
	void onAccept(const std::shared_ptr<Listener>& listener, const std::error_code& error, asio::ip::tcp::socket socket);
	void acceptConnection(const Listener& listener, asio::ip::tcp::socket socket);
	void scheduleAcceptRetry(const std::shared_ptr<Listener>& listener);
	void waitForDisconnectCallbacks(std::chrono::milliseconds timeout);
	void closeListeners();
	void stopThreads(std::chrono::milliseconds timeout);
	bool isOwnThread() const;

	const std::shared_ptr<detail::ServerContext> context;
	const int32_t readWriteThreads;
	const int32_t disconnectThreadCount;
	const std::vector<ServerCfg> cfgs;

	/** guards state and connect/shutdown */
	mutable std::mutex stateMutex;
	State state = State::NEW;
	std::vector<std::shared_ptr<Listener>> listeners;
	std::optional<asio::executor_work_guard<asio::io_context::executor_type>> ioWork;
	std::optional<asio::executor_work_guard<asio::io_context::executor_type>> disconnectWork;
	std::vector<std::thread> ioThreads;
	std::vector<std::thread> disconnectThreads;
};

} // namespace aion::commons::network
