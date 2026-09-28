#pragma once

#include <atomic>
#include <memory>
#include <mutex>
#include <string>

#include "aion/commons/network/AConnection.h"
#include "aion/commons/network/PacketProcessor.h"
#include "aion/loginserver/PingPongTask.h"
#include "aion/loginserver/network/gameserver/GsServerPacket.h"
#include "aion/loginserver/utils/ScheduledExecutor.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::loginserver {
class GameServerInfo;
}

namespace aion::loginserver::network::gameserver {

/**
 * Object representing connection between LoginServer and GameServer.
 * <p>
 * <b>Packet execution.</b> Deviation: Java executes game server packets on a cached thread pool (PACKET_EXECUTOR) in no particular order, so
 * e.g. a CM_ACCOUNT_DISCONNECTED could be processed before the CM_ACCOUNT_AUTH sent before it, and packets of one connection run concurrently on
 * unsynchronized state. Here they are executed by a PacketProcessor (see NetConnector), which runs the packets of different game servers in
 * parallel but those of one game server one at a time in the order they were received. The exception is CM_GS_PONG, which runs directly on the
 * IO thread (GsClientPacket::isRunOnReceive): queued behind a slow packet (e.g. a CM_ACCOUNT_LIST with thousands of accounts loaded from the
 * database), the pongs would not be counted and the PingPongTask would close a game server that answers every ping, while in Java they run
 * concurrently with the slow packet.
 * <p>
 * <b>Threads.</b> The state is atomic and the GameServerInfo is guarded by a leaf mutex, since both are read by other threads (Aion client
 * packets, the ping task, IO threads writing packets).
 * <p>
 * Java: com.aionemu.loginserver.network.gameserver.GsConnection
 *
 * @author -Nemesiss-
 */
class GsConnection : public commons::network::AConnection<GsServerPacket> {
public:
	using Processor = commons::network::PacketProcessor<GsConnection>;

	/** Possible states of GsConnection */
	enum class State {
		/** Means that GameServer just connect, but is not authenticated yet */
		CONNECTED,
		/** GameServer is authenticated */
		AUTHED
	};

	/**
	 * @param socket the accepted socket
	 * @param server the server the connection belongs to
	 * @param processor executes the received packets (Java: static PACKET_EXECUTOR)
	 * @param pingPongExecutor runs the PingPongTask (Java: static PINGPONG_EXECUTOR)
	 */
	GsConnection(asio::ip::tcp::socket socket, commons::network::NioServer& server, std::shared_ptr<Processor> processor,
		std::shared_ptr<utils::ScheduledExecutor> pingPongExecutor);

	/** @return Current state of this connection. */
	State getState() const noexcept { return state.load(); }

	/** Set current state of this connection. Setting AUTHED starts the PingPongTask. */
	void setState(State state);

	/** @return GameServerInfo for this GsConnection or nullptr if this GsConnection is not authenticated yet (or disconnected). */
	std::shared_ptr<GameServerInfo> getGameServerInfo() const;

	/** Set GameServerInfo for this GsConnection. */
	void setGameServerInfo(std::shared_ptr<GameServerInfo> gameServerInfo);

	PingPongTask& getPingPongTask() noexcept { return pingPongTask; }

	/** @return String info about this connection: "Gameserver #&lt;id&gt; &lt;ip&gt;" or "Gameserver &lt;ip&gt;" */
	std::string toString() const override;

protected:
	bool processData(commons::utils::ByteBuffer& data) override;
	bool writeData(commons::utils::ByteBuffer& data) override;
	void onDisconnect() override;
	void onServerClose() override;
	void initialized() override;

private:
	const std::shared_ptr<Processor> processor;
	const std::shared_ptr<utils::ScheduledExecutor> pingPongExecutor;

	/** Current state of this connection */
	std::atomic<State> state = State::CONNECTED;

	/** guards gameServerInfo (leaf lock) */
	mutable std::mutex fieldMutex;
	/** GameServerInfo for this GsConnection. */
	std::shared_ptr<GameServerInfo> gameServerInfo;

	PingPongTask pingPongTask{*this};
};

/** Java: State.toString() */
const char* toString(GsConnection::State state) noexcept;

} // namespace aion::loginserver::network::gameserver
