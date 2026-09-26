#pragma once

#include <concepts>
#include <deque>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>

#include "aion/commons/network/AConnection.h"
#include "aion/commons/utils/ByteBuffer.h"
#include "aion/gameserver/network/chatserver/ChatServerConnection_State.h"
#include "aion/gameserver/network/chatserver/CsServerPacket.h"
#include "aion/gameserver/network/chatserver/fwd.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/sched/SerialExecutor.h"
#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::chatserver {

/**
 * C++ (runtime-architecture.md §11): like LoginServerConnection - created with std::make_shared by ChatServer::connect; IO callbacks open a
 * TaskScope; received packets run in receive order on a per-link SerialExecutor (§1.4); server packets are written lazily by writeData.
 *
 * @author ATracer, Neon
 */
// fieldmap.toml: sendMsgQueue is the commons AConnection<CsServerPacket> queue (base class member, guarded by guard)
class ChatServerConnection : public commons::network::AConnection<CsServerPacket> {
public:
	/** Possible states of CsConnection (generated enum ChatServerConnection_State) */
	using State = ChatServerConnection_State;

private:
	/** Current state of this connection */
	runtime::Field<ChatServerConnection::State> state{};
	/** C++ only: runs the received packets of this link one at a time in receive order (runtime-architecture.md §1.4) */
	runtime::SerialExecutor packetExecutor; // fieldmap.toml: C++-only thread-safe executor object (no retained game object), set once in the constructor

public:
	ChatServerConnection(asio::ip::tcp::socket socket, commons::network::NioServer& server);
	~ChatServerConnection() override;

	using AConnection::sendPacket;

	/** C++ only: sendPacket for packet temporaries */
	template <std::derived_from<CsServerPacket> P>
	void sendPacket(P&& packet) {
		sendPacket(std::shared_ptr<CsServerPacket>(std::make_shared<std::remove_cvref_t<P>>(std::forward<P>(packet))));
	}

protected:
	void initialized() override;

	/** Java final */
	std::deque<std::shared_ptr<CsServerPacket>>& getSendMsgQueue() noexcept { return sendMsgQueue; }

public:
	bool processData(commons::utils::ByteBuffer& data) override;

protected:
	bool writeData(commons::utils::ByteBuffer& data) override final;

	void onDisconnect() override final;

	void onServerClose() override final;

public:
	State getState() const { return state.get(); }

	void setState(State value) { state.set(value); }

	std::string toString() const override;
};

} // namespace aion::gameserver::network::chatserver
