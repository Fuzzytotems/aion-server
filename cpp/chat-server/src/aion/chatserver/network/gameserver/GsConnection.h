#pragma once

#include <atomic>
#include <memory>
#include <string>
#include <string_view>

#include "aion/chatserver/network/gameserver/GsServerPacket.h"
#include "aion/commons/network/AConnection.h"
#include "aion/commons/network/PacketProcessor.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::chatserver::network::gameserver {

/**
 * The connection of a game server.
 * <p>
 * <b>Packet execution.</b> Deviation: Java executes game server packets on a cached thread pool (PACKET_EXECUTOR) in no particular order, so a
 * CM_PLAYER_LOGOUT could run before the CM_PLAYER_AUTH sent before it. Here they are executed by a PacketProcessor (owned by NettyServer, like
 * the login server's NetConnector), which runs the packets of one game server one at a time in the order they were received.
 * <p>
 * Java: com.aionemu.chatserver.network.gameserver.GsConnection
 *
 * @author KID
 */
class GsConnection : public commons::network::AConnection<GsServerPacket> {
public:
	using Processor = commons::network::PacketProcessor<GsConnection>;

	enum class GameServerConnectionState {
		CONNECTED,
		AUTHED
	};

	/** @param processor executes the received packets (Java: static PACKET_EXECUTOR) */
	GsConnection(asio::ip::tcp::socket socket, commons::network::NioServer& server, std::shared_ptr<Processor> processor);

	GameServerConnectionState getState() const noexcept { return state.load(); }

	void setState(GameServerConnectionState value) noexcept { state.store(value); }

	/** @return "Gameserver &lt;ip&gt;" */
	std::string toString() const override;

protected:
	bool processData(commons::utils::ByteBuffer& data) override;
	bool writeData(commons::utils::ByteBuffer& data) override;
	/** Java: GameServerService.setOffline() (for every game server connection, also one that never authenticated) */
	void onDisconnect() override;
	/** Java: close() and PACKET_EXECUTOR.shutdown(); the processor is shut down by NettyServer after the server */
	void onServerClose() override;
	void initialized() override;

private:
	const std::shared_ptr<Processor> processor;
	std::atomic<GameServerConnectionState> state = GameServerConnectionState::CONNECTED;
};

/** Java: GameServerConnectionState.name() */
std::string_view toString(GsConnection::GameServerConnectionState state) noexcept;

} // namespace aion::chatserver::network::gameserver
