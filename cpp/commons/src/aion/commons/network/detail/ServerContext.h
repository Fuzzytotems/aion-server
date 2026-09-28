#pragma once

#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <unordered_set>

#include <asio/io_context.hpp>

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::commons::network {
class AConnectionBase;
}

namespace aion::commons::network::detail {

/**
 * State shared between a NioServer and its connections (internal). Connections keep it alive through a shared_ptr, so the io_context that
 * owns their sockets outlives them even if the NioServer object is destroyed first.
 */
struct ServerContext {
	/** runs all socket IO (Java: the Dispatcher selector threads) */
	asio::io_context ioContext;
	/** runs onDisconnect callbacks if no dcExecutor is set (Java: dcExecutor) */
	asio::io_context disconnectContext;
	/** optional executor for onDisconnect callbacks, set before the server starts (Java: dcExecutor) */
	std::function<void(std::function<void()>)> dcExecutor;

	/** guards activeConnections, acceptingConnections, pendingDisconnects and runningIoThreads */
	std::mutex mutex;
	/** notified whenever a connection is unregistered or an IO thread exits */
	std::condition_variable stateChanged;
	/** registered connections that are not disconnected yet (Java: the connections attached to the selectors' keys) */
	std::unordered_set<std::shared_ptr<AConnectionBase>> activeConnections;
	/** false once the server shuts down: new connections are rejected */
	bool acceptingConnections = true;
	/** onDisconnect callbacks that were dispatched and did not finish yet */
	int pendingDisconnects = 0;
	/** number of IO threads still running */
	int runningIoThreads = 0;
};

} // namespace aion::commons::network::detail
