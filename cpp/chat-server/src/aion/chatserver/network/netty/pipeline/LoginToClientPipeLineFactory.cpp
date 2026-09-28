#include "aion/chatserver/network/netty/pipeline/LoginToClientPipeLineFactory.h"

#include <utility>

#include "aion/chatserver/network/aion/ClientPacketHandler.h"
#include "aion/chatserver/network/netty/handler/ClientChannelHandler.h"
#include "aion/chatserver/network/netty/pipeline/ExecutionHandler.h"

namespace aion::chatserver::network::netty::pipeline {

LoginToClientPipeLineFactory::LoginToClientPipeLineFactory(std::shared_ptr<const aion::ClientPacketHandler> clientPacketHandler)
	: clientPacketHandler(std::move(clientPacketHandler)), executionHandler(std::make_shared<ExecutionHandler>(THREADS_MAX)) {
}

LoginToClientPipeLineFactory::~LoginToClientPipeLineFactory() {
	try {
		executionHandler->shutdown(std::chrono::milliseconds(0)); // the executor's threads keep it alive until they are stopped
	} catch (...) {
		// shutdown only throws if a thread cannot be joined; nothing is left to do here
	}
}

std::shared_ptr<commons::network::AConnectionBase> LoginToClientPipeLineFactory::getPipeline(asio::ip::tcp::socket socket,
	commons::network::NioServer& server) const {
	return std::make_shared<handler::ClientChannelHandler>(std::move(socket), server, clientPacketHandler, executionHandler);
}

void LoginToClientPipeLineFactory::shutdown(std::chrono::milliseconds timeout) {
	executionHandler->shutdown(timeout);
}

} // namespace aion::chatserver::network::netty::pipeline
