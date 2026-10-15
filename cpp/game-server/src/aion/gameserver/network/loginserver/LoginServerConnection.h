#pragma once

#include <concepts>
#include <deque>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>

#include "aion/commons/network/AConnection.h"
#include "aion/commons/utils/ByteBuffer.h"
#include "aion/gameserver/network/loginserver/LoginServerConnection_State.h"
#include "aion/gameserver/network/loginserver/LsServerPacket.h"
#include "aion/gameserver/network/loginserver/fwd.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/sched/SerialExecutor.h"
#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::loginserver {

/**
 * Object representing connection between LoginServer and GameServer.
 * <p>
 * C++ (runtime-architecture.md §11): created with std::make_shared by LoginServer::connect (commons convention for outbound connections).
 * initialized(), processData and writeData run on IO threads inside a TaskScope. Received packets are read on the IO strand (Java: readImpl on
 * the dispatcher) and run in receive order on a per-link SerialExecutor on the instant pool (§1.4; Java executes them unordered on the
 * ThreadPoolManager). The send queue is the commons AConnection queue (Java: getSendMsgQueue()); writeData writes the queued packets. LoginServer
 * serializes its packets on the sending thread first (LoginServer.cpp SerializedLsPacket), so writeData only copies bytes.
 *
 * @author -Nemesiss-, Neon
 */
// fieldmap.toml: sendMsgQueue is the commons AConnection<LsServerPacket> queue (base class member, guarded by guard)
class LoginServerConnection : public commons::network::AConnection<LsServerPacket> {
public:
	/** Possible states of GsConnection (generated enum LoginServerConnection_State) */
	using State = LoginServerConnection_State;

private:
	/** Current state of this connection */
	runtime::Field<LoginServerConnection::State> state{};
	/** C++ only: runs the received packets of this link one at a time in receive order (runtime-architecture.md §1.4) */
	runtime::SerialExecutor packetExecutor; // fieldmap.toml: C++-only thread-safe executor object (no retained game object), set once in the constructor

public:
	LoginServerConnection(asio::ip::tcp::socket socket, commons::network::NioServer& server);
	~LoginServerConnection() override;

	using AConnection::sendPacket;

	/** C++ only: sendPacket for packet temporaries (`con->sendPacket(SM_X(...))`) */
	template <std::derived_from<LsServerPacket> P>
	void sendPacket(P&& packet) {
		sendPacket(std::shared_ptr<LsServerPacket>(std::make_shared<std::remove_cvref_t<P>>(std::forward<P>(packet))));
	}

protected:
	void initialized() override;

	/** Java final */
	std::deque<std::shared_ptr<LsServerPacket>>& getSendMsgQueue() noexcept { return sendMsgQueue; }

public:
	/**
	 * Called by the IO strand. ByteBuffer data contains one packet that should be processed.
	 *
	 * @return True if data was processed correctly, False if some error occurred and connection should be closed NOW.
	 */
	bool processData(commons::utils::ByteBuffer& data) override;

protected:
	/**
	 * Called by the IO strand with guard held, and will be repeated till return false.
	 *
	 * @return True if data was written to buffer, False indicating that there are not any more data to write.
	 */
	bool writeData(commons::utils::ByteBuffer& data) override final;

	void onDisconnect() override final;

	void onServerClose() override final;

public:
	/** @return Current state of this connection. */
	State getState() const { return state.get(); }

	/** @param state Set current state of this connection. */
	void setState(State value) { state.set(value); }

	/** @return String info about this connection */
	std::string toString() const override;
};

} // namespace aion::gameserver::network::loginserver
