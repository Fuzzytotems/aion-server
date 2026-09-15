#pragma once

#include <concepts>
#include <cstdint>
#include <deque>
#include <memory>
#include <string>
#include <string_view>

#include "aion/commons/network/AConnection.h"
#include "aion/commons/network/PacketProcessor.h"
#include "aion/commons/network/packet/BasePacket.h"
#include "aion/commons/utils/ByteBuffer.h"
#include "aion/gameserver/runtime/collections/ConcurrentHashMap.h"
#include "aion/gameserver/runtime/fields/Atomic.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/runtime/sched/Future.h"
#include "aion/gameserver/runtime/sync/Monitor.h"
#include "aion/gameserver/model/account/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/network/Crypt.h"
#include "aion/gameserver/network/aion/AionConnection_State.h"
#include "aion/gameserver/network/aion/SerializedBody.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion {

/**
 * Object representing connection between GameServer and Aion Client.
 * <p>
 * Hub header (docs/design/hub-headers.md §12). Unlike the other hubs this header includes Asio through its direct base class
 * commons::network::AConnection; hubs and packets name the class through fwd.h only (`AionConnection*` parameters,
 * `std::shared_ptr<AionConnection>` returns and `Field<std::shared_ptr<AionConnection>>` members). Connections are created with
 * std::make_shared by the connection factory (commons convention).
 * <p>
 * Eager packets (runtime-architecture.md §8): the send queue holds SerializedBody elements ordered by their serialization sequence number
 * (§8.4). sendPacket serializes on the calling thread (per recipient or once) and enqueues; writeData runs on the IO strand with `guard` held,
 * prepends the length and encrypts (Crypt's first encrypt, of SM_KEY, only enables the crypt, like Java). The base class'
 * `sendPacket(std::shared_ptr<SerializedBody>)` and `close(std::shared_ptr<SerializedBody>)` are hidden by the packet overloads below.
 * <p>
 * Threads (§11): the constructor, initialized(), processData and writeData run on IO threads, each inside a TaskScope, and contain no game logic;
 * onDisconnect runs on the instant pool. Tasks scheduled for a connection capture std::weak_ptr<AionConnection>, so the ConnectionAliveChecker
 * is created in initialized() (weak_from_this() is empty during construction), not in the constructor as in Java.
 *
 * @author -Nemesiss-
 */
class AionConnection : public commons::network::AConnection<SerializedBody> {
public:
	/** Possible states of AionConnection (generated enum AionConnection_State) */
	using State = AionConnection_State;

private:
	/** Java private inner class: closes hanged up connections (scheduled at fixed rate CM_PING.CLIENT_PING_INTERVAL). */
	class ConnectionAliveChecker : public runtime::RefCounted {
		AION_MAKE_REF_FRIEND
	public:
		const runtime::FutureRef task;
		const std::weak_ptr<AionConnection> aionConnection;

		/** Java: new ConnectionAliveChecker() (schedules itself). @throws IllegalStateException if the connection already has one */
		static runtime::Ref<ConnectionAliveChecker> create(std::weak_ptr<AionConnection> aionConnection);

		void stop();

		/** Java Runnable.run */
		void run();

	private:
		/**
		 * C++ only: the body of run() for the given connection. The scheduled task calls it with its own copy of the weak_ptr: `task` is
		 * initialized (and the task scheduled) before the `aionConnection` member, so the task must not read that member.
		 */
		static void checkAlive(const std::weak_ptr<AionConnection>& aionConnection);

	protected:
		explicit ConnectionAliveChecker(std::weak_ptr<AionConnection> aionConnection);
		~ConnectionAliveChecker() override;
	};

	/** Java: private static final PacketProcessor<AionConnection> packetProcessor, created on first use from NetworkConfig and ThreadConfig */
	static commons::network::PacketProcessor<AionConnection>& packetProcessor();

	/** Server Packet "to send" Queue, ordered by SerializedBody::seq, guarded by guard */
	std::deque<SerializedBody> sendMsgQueue;
	/** Current state of this connection */
	runtime::Field<AionConnection::State> state{};
	/** AionClient is authenticating by passing to GameServer id of account. */
	runtime::AtomicReference<runtime::Ref<model::account::Account>> account{AION_LOCK_CLASS(AionConnection::account)};
	/** Crypt that will encrypt/decrypt packets (IO strand only). */
	Crypt crypt{};
	/** active Player that owner of this connection is playing [entered game] */
	runtime::AtomicReference<runtime::Ref<model::gameobjects::player::Player>> activePlayer{AION_LOCK_CLASS(AionConnection::activePlayer)};
	runtime::Field<int64_t> lastClientMessageTime{};
	runtime::Field<int64_t> lastPingTime{};
	runtime::Field<int32_t> pingFailCount{};
	static constexpr int32_t MAX_CORRUPT_PACKETS_BEFORE_DISCONNECT = 3;
	runtime::Field<int32_t> corruptPackets{0};
	runtime::Field<std::string> macAddress{};
	runtime::Field<std::string> hddSerial{};
	runtime::Field<runtime::Ref<ConnectionAliveChecker>> connectionAliveChecker{};
	/** packet flood filter (C++: always created; Java creates it only if PffConfig.PFF_MODE > 0 and thresholds exist, and checks for null) */
	runtime::ConcurrentHashMap<int32_t, int64_t> pffRequests{AION_LOCK_CLASS(AionConnection::pffRequests#stripe)};
	/** C++ only: the Java object monitor of the connection (`synchronized (this)` in safeLogout, runtime-architecture.md §3.4) */
	// fieldmap.toml [cpp_members]: C++-only object monitor (AionConnection is not RefCounted)
	mutable runtime::Monitor monitor_{AION_LOCK_CLASS(AionConnection::monitor)};

public:
	/**
	 * Java: AionConnection(SocketChannel sc, Dispatcher d) with read and write buffers of 8192 * 4 bytes. Runs on the accepting IO thread.
	 */
	AionConnection(asio::ip::tcp::socket socket, commons::network::NioServer& server);
	~AionConnection() override;

	/** C++ only: `SYNCHRONIZED(connection)` locks this Monitor (Java: synchronized on the connection object) */
	runtime::Monitor& monitor() const noexcept { return monitor_; }

	/**
	 * Java AConnection.sendPacket(AionServerPacket): serializes the packet for this connection on the calling thread and enqueues it. Ignored if
	 * the connection is closing or closed.
	 */
	void sendPacket(AionServerPacket& packet);

	/** C++ only: sendPacket for packet temporaries (`con->sendPacket(SM_X(...))`) */
	template <std::derived_from<AionServerPacket> P>
	void sendPacket(P&& packet) {
		sendPacket(static_cast<AionServerPacket&>(packet));
	}

	/** C++ only (runtime-architecture.md §8.2): inserts an already serialized packet by sequence number and requests a write. */
	void enqueue(SerializedBody body);

	using AConnectionBase::close;

	/**
	 * Java AConnection.close(AionServerPacket): the close packet is serialized now and sent before closing; previously queued and future packets
	 * are not.
	 */
	void close(AionServerPacket& closePacket);

	/** C++ only: close for packet temporaries (`con->close(SM_X(...))`) */
	template <std::derived_from<AionServerPacket> P>
	void close(P&& closePacket) {
		close(static_cast<AionServerPacket&>(closePacket));
	}

protected:
	/** Sends SM_KEY and starts the ConnectionAliveChecker (C++: moved here from the constructor). */
	void initialized() override;

public:
	/**
	 * Enable crypt key - generate random key that will be used to encrypt second server packet [first one is unencrypted] and decrypt client packets.
	 * This method is called from SM_KEY server packet, that packet sends key to aion client. Java final.
	 *
	 * @return "false key" that should by used by aion client to encrypt/decrypt packets.
	 */
	int32_t enableCryptKey();

protected:
	/** Java: getSendMsgQueue() (final override). The caller must hold guard. */
	std::deque<SerializedBody>& getSendMsgQueue() noexcept { return sendMsgQueue; }

	/**
	 * Called by the IO strand. ByteBuffer data contains one packet that should be processed.
	 *
	 * @return True if data was processed correctly, False if some error occurred and connection should be closed NOW.
	 */
	bool processData(commons::utils::ByteBuffer& data) override final;

	/**
	 * Called by the IO strand with guard held, and will be repeated till return false.
	 *
	 * @return True if data was written to buffer, False indicating that there are not any more data to write.
	 */
	bool writeData(commons::utils::ByteBuffer& data) override final;

private:
	bool canReceivePacketInfoInChat();

	void sendPacketInfo(commons::network::packet::BasePacket& packet);

public:
	/** Java package-private */
	void sendUnknownClientPacketInfo(int32_t opCode);

protected:
	void onDisconnect() override final;

	void onServerClose() override final;

private:
	/** Java: synchronized (this) { ... } */
	void safeLogout();

public:
	/** Encrypt packet. Java final. (IO strand only) */
	void encrypt(commons::utils::ByteBuffer& buf);

	/** Current state of this connection. Java final. */
	State getState() const { return state.get(); }

	/** Sets the state of this connection */
	void setState(State value) { state.set(value); }

	/** Returns account object associated with this connection (null before authentication) */
	runtime::Ptr<model::account::Account> getAccount();

	/** Sets account object associated with this connection. Java: Objects.requireNonNull(account, "Account can't be null") */
	void setAccount(model::account::Account& account);

	/**
	 * Sets Active player to new value. Update connection state to correct value.
	 *
	 * @param player
	 *          the entering player, or null when leaving the world
	 * @return True if active player was set to new value.
	 */
	bool setActivePlayer(runtime::Ptr<model::gameobjects::player::Player> player);

	/** Return active player or null. */
	runtime::Ptr<model::gameobjects::player::Player> getActivePlayer();

	int64_t getLastClientMessageTime() const { return lastClientMessageTime.get(); }

	int64_t getLastPingTime() const { return lastPingTime.get(); }

	void setLastPingTime(int64_t time) { lastPingTime.set(time); }

	int32_t increaseAndGetPingFailCount();

	void resetPingFailCount() { pingFailCount.set(0); }

	void setMacAddress(std::string_view mac) { macAddress.set(std::string(mac)); }

	std::string getMacAddress() const { return macAddress.get(); }

	std::string getHddSerial() const { return hddSerial.get(); }

	void setHddSerial(std::string_view value) { hddSerial.set(std::string(value)); }

	std::string toString() const override;
};

} // namespace aion::gameserver::network::aion
