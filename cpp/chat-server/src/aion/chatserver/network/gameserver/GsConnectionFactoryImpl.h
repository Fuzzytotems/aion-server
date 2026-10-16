#pragma once

#include <memory>

#include "aion/chatserver/network/gameserver/GsConnection.h"
#include "aion/commons/network/ConnectionFactory.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::chatserver::network::gameserver {

/**
 * ConnectionFactory implementation that will be creating GsConnections. Used as the commons ConnectionFactory of the game server listener.
 * <p>
 * Java: com.aionemu.chatserver.network.gameserver.GsConnectionFactoryImpl
 *
 * @author -Nemesiss-
 */
class GsConnectionFactoryImpl {
public:
	/** @param processor the packet processor of the new connections (Java: GsConnection.PACKET_EXECUTOR) */
	explicit GsConnectionFactoryImpl(std::shared_ptr<GsConnection::Processor> processor) : processor(std::move(processor)) {}

	/** Java: create(socket, dispatcher) - a new GsConnection for the accepted socket */
	std::shared_ptr<commons::network::AConnectionBase> operator()(asio::ip::tcp::socket socket, commons::network::NioServer& server) const {
		return std::make_shared<GsConnection>(std::move(socket), server, processor);
	}

private:
	std::shared_ptr<GsConnection::Processor> processor;
};

} // namespace aion::chatserver::network::gameserver
