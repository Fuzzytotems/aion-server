#pragma once

#include <atomic>
#include <memory>
#include <string>
#include <string_view>

#include "aion/chatserver/network/gameserver/GsServerPacket.h"
#include "aion/chatserver/network/netty/NettyConnection.h"

namespace aion::chatserver::network::gameserver {

/**
 * The connection of a game server. Its packets run one after another on its packet executor, in the order they were received.
 * <p>
 * Java: com.aionemu.chatserver.network.gameserver.GsConnection
 *
 * @author KID
 */
class GsConnection : public netty::NettyConnection<GsServerPacket> {
public:
	enum class GameServerConnectionState {
		CONNECTED,
		AUTHED
	};

	GsConnection(std::shared_ptr<netty::ConnectionHandler> channel, Executor packetExecutor);

	GameServerConnectionState getState() const noexcept { return state.load(); }

	void setState(GameServerConnectionState value) noexcept { state.store(value); }

	/** @return "Game server &lt;ip&gt;" */
	std::string toString() const override;

protected:
	std::unique_ptr<commons::network::packet::ClientPacketBase> createPacket(commons::utils::ByteBuffer& data) override;
	void writePacket(const GsServerPacket& packet, commons::utils::ByteBuffer& buffer) override;
	int32_t getMaxPendingPackets() const override { return 10'000; }
	void onConnect() override;
	/** Java: GameServerService.setOffline() (for every game server connection, also one that never authenticated) */
	void onDisconnect() override;

private:
	std::atomic<GameServerConnectionState> state = GameServerConnectionState::CONNECTED;
};

/** Java: GameServerConnectionState.name() */
std::string_view toString(GsConnection::GameServerConnectionState state) noexcept;

} // namespace aion::chatserver::network::gameserver
