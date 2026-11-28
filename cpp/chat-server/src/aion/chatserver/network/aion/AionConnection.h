#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>

#include "aion/chatserver/network/aion/AbstractServerPacket.h"
#include "aion/commons/network/netty/NettyConnection.h"

namespace aion::chatserver::model {
class ChatClient;
}

namespace aion::chatserver::network::aion {

/**
 * Connection of an Aion game client to the chat server.
 * <p>
 * C++: the ChatClient and the connection reference each other (Java: a cycle the garbage collector resolves); onDisconnect drops the
 * ChatClient, which breaks the cycle once no packet of the connection can run anymore.
 * <p>
 * Java: com.aionemu.chatserver.network.aion.AionConnection
 */
class AionConnection : public commons::network::netty::NettyConnection<AbstractServerPacket> {
public:
	enum class State {
		CONNECTED,
		AUTHED
	};

	AionConnection(std::shared_ptr<commons::network::netty::ConnectionHandler> channel, Executor packetExecutor);
	~AionConnection() override;

	State getState() const noexcept { return state.load(); }

	void setState(State value) noexcept { state.store(value); }

	std::shared_ptr<model::ChatClient> getChatClient() const;

	void setChatClient(std::shared_ptr<model::ChatClient> chatClient);

	/** @return "Client &lt;ip&gt;" before the authentication, then "&lt;chat client&gt; (&lt;ip&gt;)" */
	std::string toString() const override;

protected:
	std::unique_ptr<commons::network::packet::ClientPacketBase> createPacket(commons::utils::ByteBuffer& data) override;
	void writePacket(const AbstractServerPacket& packet, commons::utils::ByteBuffer& buffer) override;
	bool isAuthenticated() const override;
	/** @return NetworkConfig::CLIENT_AUTH_TIMEOUT_SECONDS in milliseconds */
	int64_t getAuthTimeoutMillis() const override;
	int32_t getMaxPendingPackets() const override { return 100; }
	int64_t getMaxPendingWriteBytes() const override { return 256 * 1024; }
	int64_t getMaxSendStallMillis() const override { return 60'000; }
	void onConnect() override;
	void onDisconnect() override;

private:
	std::atomic<State> state = State::CONNECTED;

	/** guards chatClient */
	mutable std::mutex chatClientMutex;
	std::shared_ptr<model::ChatClient> chatClient;
};

/** Java: State.name() */
std::string_view toString(AionConnection::State state) noexcept;

} // namespace aion::chatserver::network::aion
