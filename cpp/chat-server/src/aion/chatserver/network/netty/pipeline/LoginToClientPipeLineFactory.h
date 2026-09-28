#pragma once

#include <chrono>
#include <cstdint>
#include <memory>

#include <asio/ip/tcp.hpp>

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::commons::network {
class AConnectionBase;
class NioServer;
} // namespace aion::commons::network

namespace aion::chatserver::network::aion {
class ClientPacketHandler;
}

namespace aion::chatserver::network::netty::pipeline {

class ExecutionHandler;

/**
 * Creates the connection of an accepted Aion client: a ClientChannelHandler sharing the ClientPacketHandler and the ExecutionHandler with all
 * other client connections. Used as the ConnectionFactory of the client listener (see NettyServer).
 * <p>
 * Java: com.aionemu.chatserver.network.netty.pipeline.LoginToClientPipeLineFactory (the pipeline "framedecoder", "packetdecoder",
 * "packetencoder", "executor", "handler" is implemented by ClientChannelHandler, see there)
 *
 * @author ATracer
 */
class LoginToClientPipeLineFactory {
public:
	static constexpr int32_t THREADS_MAX = 10;
	/** not enforced, see ExecutionHandler */
	static constexpr int32_t MEMORY_PER_CHANNEL = 1048576;
	/** not enforced, see ExecutionHandler */
	static constexpr int32_t TOTAL_MEMORY = 134217728;
	/** Netty: the keep-alive time of idle executor threads; the ExecutionHandler keeps its threads until it is shut down */
	static constexpr int32_t TIMEOUT = 100;

	explicit LoginToClientPipeLineFactory(std::shared_ptr<const aion::ClientPacketHandler> clientPacketHandler);

	/** Stops the executor like shutdown() with a zero timeout, if that did not happen yet. */
	~LoginToClientPipeLineFactory();

	LoginToClientPipeLineFactory(const LoginToClientPipeLineFactory&) = delete;
	LoginToClientPipeLineFactory& operator=(const LoginToClientPipeLineFactory&) = delete;

	/** Java: getPipeline() - the connection of an accepted socket */
	std::shared_ptr<commons::network::AConnectionBase> getPipeline(asio::ip::tcp::socket socket, commons::network::NioServer& server) const;

	/**
	 * C++ addition: waits up to timeout until the events queued by the client connections ran (e.g. the disconnect of every client after the
	 * server shut down), then stops the executor threads; a thread still running an event is left to end after it (Java: the executor is never
	 * shut down).
	 */
	void shutdown(std::chrono::milliseconds timeout);

	/** C++ addition for tests: the executor of the client events */
	const std::shared_ptr<ExecutionHandler>& getExecutionHandler() const noexcept { return executionHandler; }

private:
	const std::shared_ptr<const aion::ClientPacketHandler> clientPacketHandler;
	const std::shared_ptr<ExecutionHandler> executionHandler;
};

} // namespace aion::chatserver::network::netty::pipeline
