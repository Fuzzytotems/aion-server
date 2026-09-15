#include "aion/gameserver/network/aion/AionConnection.h"

#include <algorithm>
#include <chrono>
#include <typeinfo>
#include <utility>

#include "aion/commons/configs/CommonsConfig.h"
#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/utils/Exception.h"
#include "aion/commons/utils/TimeUtils.h"
#include "aion/commons/utils/concurrent/ExecuteWrapper.h"
#include "aion/commons/utils/concurrent/RunnableStatsManager.h"
#include "aion/gameserver/configs/main/ThreadConfig.h"
#include "aion/gameserver/configs/network/NetworkConfig.h"
#include "aion/gameserver/configs/network/PffConfig.h"
#include "aion/gameserver/controllers/movement/PlayerMoveController.h"
#include "aion/gameserver/dataholders/loadingutils/EnumTraits.h"
#include "aion/gameserver/model/ChatType.h"
#include "aion/gameserver/model/account/Account.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/network/aion/AionClientPacket.h"
#include "aion/gameserver/network/aion/AionClientPacketFactory.h"
#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"
#include "aion/gameserver/network/aion/serverpackets/SM_KEY.h"
#include "aion/gameserver/network/aion/serverpackets/SM_MESSAGE.h"
#include "aion/gameserver/network/loginserver/LoginServer.h"
#include "aion/gameserver/runtime/lifetime/TaskScope.h"
#include "aion/gameserver/services/player/PlayerLeaveWorldService.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion {

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.network.aion.AionConnection");

namespace {

using configs::network::PffConfig;
using model::gameobjects::player::Player;

/** Java: CM_PING.CLIENT_PING_INTERVAL (client sends this packet every 180 seconds). TODO(P5-00): use CM_PING::CLIENT_PING_INTERVAL */
constexpr int32_t CLIENT_PING_INTERVAL = 180 * 1000;

/**
 * Java: GameServer.isShuttingDownSoon() (ShutdownHook running with at most 30 seconds left). TODO(P5-14): call
 * GameServer::isShuttingDownSoon() once GameServer.h exists; until then no shutdown countdown exists in the C++ server.
 */
bool isGameServerShuttingDownSoon() {
	return false;
}

/** Java: the AionServerPacket.toString() of a queued packet (the queue holds serialized bodies) */
std::string packetNameOf(int32_t opCode) {
	const ServerPacketsOpcodes::Entry* entry = ServerPacketsOpcodes::findByOpcode(opCode);
	return commons::network::packet::BasePacket::toFormattedPacketNameString(3, opCode, entry != nullptr ? entry->name : "SM_CUSTOM_PACKET");
}

} // namespace

// ------------------------------------------------------------------------------------------------------------------ ConnectionAliveChecker

// lint: L7 weak_ptr::lock() takes no monitor
runtime::Ref<AionConnection::ConnectionAliveChecker> AionConnection::ConnectionAliveChecker::create(
	std::weak_ptr<AionConnection> aionConnectionValue) {
	if (std::shared_ptr<AionConnection> con = aionConnectionValue.lock(); con && con->connectionAliveChecker.get())
		throw commons::utils::IllegalStateException("ConnectionAliveChecker for " + con->toString() + " is already assigned.");
	return runtime::makeRef<ConnectionAliveChecker>(std::move(aionConnectionValue));
}

AionConnection::ConnectionAliveChecker::ConnectionAliveChecker(std::weak_ptr<AionConnection> aionConnectionValue)
	// the task is scheduled before the aionConnection member is initialized (declaration order): it runs on its own copy of the weak_ptr
	: task(utils::ThreadPoolManager::getInstance().scheduleAtFixedRate(
			runtime::Pin(this), [connection = aionConnectionValue] { checkAlive(connection); }, CLIENT_PING_INTERVAL, CLIENT_PING_INTERVAL)),
		aionConnection(std::move(aionConnectionValue)) {
}

AionConnection::ConnectionAliveChecker::~ConnectionAliveChecker() = default;

void AionConnection::ConnectionAliveChecker::stop() {
	task->cancel(false);
}

void AionConnection::ConnectionAliveChecker::run() {
	checkAlive(aionConnection);
}

// lint: L7 weak_ptr::lock() takes no monitor
void AionConnection::ConnectionAliveChecker::checkAlive(const std::weak_ptr<AionConnection>& aionConnection) {
	std::shared_ptr<AionConnection> con = aionConnection.lock();
	if (!con)
		return; // C++: the connection was destroyed (Java keeps it reachable through this task)
	int64_t millisSinceLastClientPacket = commons::utils::currentTimeMillis() - con->lastClientMessageTime.get();
	if (millisSinceLastClientPacket - 5000 > CLIENT_PING_INTERVAL) {
		log.info("Closing hanged up connection of " + con->toString() + " (last sign of life was " + std::to_string(millisSinceLastClientPacket) +
			"ms ago)");
		con->close();
	}
}

// ------------------------------------------------------------------------------------------------------------------ AionConnection

commons::network::PacketProcessor<AionConnection>& AionConnection::packetProcessor() {
	using configs::network::NetworkConfig;
	// Java: private static final field, created with the class; here on first use (it reads the configs), never destroyed
	static auto* const processor = new commons::network::PacketProcessor<AionConnection>(NetworkConfig::PACKET_PROCESSOR_MIN_THREADS.load(),
		NetworkConfig::PACKET_PROCESSOR_MAX_THREADS.load(), NetworkConfig::PACKET_PROCESSOR_THREAD_SPAWN_THRESHOLD.load(),
		NetworkConfig::PACKET_PROCESSOR_THREAD_KILL_THRESHOLD.load(),
		[wrapper = commons::utils::concurrent::ExecuteWrapper(configs::main::ThreadConfig::MAXIMUM_RUNTIME_IN_MILLISEC_WITHOUT_WARNING.load())](
			commons::network::packet::ClientPacketBase& packet) {
			runtime::TaskScope scope(runtime::TaskInfo{std::source_location::current(), runtime::TaskKind::PACKET});
			wrapper.execute(packet);
		});
	return *processor;
}

AionConnection::AionConnection(asio::ip::tcp::socket socket, commons::network::NioServer& server)
	: AConnection(std::move(socket), server, 8192 * 4, 8192 * 4) {
	state.set(State::CONNECTED);

	if (log.isDebugEnabled())
		log.debug("connection from: " + getIP());

	lastClientMessageTime.set(commons::utils::currentTimeMillis());
	// C++: the ConnectionAliveChecker is created in initialized() (it needs weak_from_this()); pffRequests always exists (see processData)
}

AionConnection::~AionConnection() = default;

void AionConnection::sendPacket(AionServerPacket& packet) {
	if (isPendingClose() || isClosed())
		return; // Java: ignored if the connection is closing (checked again under guard in enqueue)
	int64_t begin = commons::utils::nanoTime();
	SerializedBody body = packet.serialize(packet.recipients() == AionServerPacket::Recipients::PER_RECIPIENT ? this : nullptr);
	if (commons::configs::CommonsConfig::RUNNABLESTATS_ENABLE.load())
		commons::utils::concurrent::RunnableStatsManager::handleStats(typeid(packet), "runImpl()", commons::utils::nanoTime() - begin);
	int32_t frameSize = static_cast<int32_t>(body.bytes->size()) + 2;
	if (frameSize > AionServerPacket::MAX_CLIENT_SUPPORTED_PACKET_SIZE)
		log.warn(packet.toString() + " contains " + std::to_string(frameSize - AionServerPacket::MAX_CLIENT_SUPPORTED_PACKET_SIZE) +
			" more bytes than the game client of " + [this] {
				runtime::Ptr<Player> player = getActivePlayer();
				return player ? player->toString() : std::string("null");
			}() + " can read");
	enqueue(std::move(body));
	// runtime-architecture.md §8.6: the packet name echo of Java's writeData runs at serialization
	if (typeid(packet) != typeid(serverpackets::SM_MESSAGE))
		sendPacketInfo(packet);
}

void AionConnection::enqueue(SerializedBody body) {
	std::lock_guard lock(guard);
	if (!canSend())
		return;
	// ordered by serialization sequence (runtime-architecture.md §8.4): packets of one thread arrive in order, so the scan usually stops at once
	auto position = sendMsgQueue.end();
	while (position != sendMsgQueue.begin() && std::prev(position)->seq > body.seq)
		--position;
	sendMsgQueue.insert(position, std::move(body));
	requestWrite();
}

void AionConnection::close(AionServerPacket& closePacket) {
	if (isPendingClose() || isClosed())
		return;
	SerializedBody body = closePacket.serialize(closePacket.recipients() == AionServerPacket::Recipients::PER_RECIPIENT ? this : nullptr);
	std::lock_guard lock(guard);
	if (!beginClose(true))
		return;
	sendMsgQueue.clear();
	sendMsgQueue.push_back(std::move(body));
}

void AionConnection::initialized() {
	runtime::TaskScope scope(runtime::TaskInfo{std::source_location::current(), runtime::TaskKind::IO});
	// SM_KEY is serialized for this connection before any other packet can be queued (runtime-architecture.md §8.5): enableCryptKey runs here,
	// before IO starts, and the first encrypt on the IO strand enables the crypt after the key packet was written unencrypted
	serverpackets::SM_KEY keyPacket;
	SerializedBody key = keyPacket.serialize(this);
	key.enablesCrypt = true;
	enqueue(std::move(key));

	std::shared_ptr<AionConnection> self = sharedFromThis();
	connectionAliveChecker.set(ConnectionAliveChecker::create(self));
}

int32_t AionConnection::enableCryptKey() {
	return crypt.enableKey();
}

bool AionConnection::processData(commons::utils::ByteBuffer& data) {
	runtime::TaskScope scope(runtime::TaskInfo{std::source_location::current(), runtime::TaskKind::IO});
	if (!crypt.isEnabled()) // skip unprocessable packet (client sends crap upon reconnect, just before Crypt gets initialized and sent via SM_KEY)
		return true;

	if (!crypt.decrypt(data.remainingSpan())) {
		if (++corruptPackets >= MAX_CORRUPT_PACKETS_BEFORE_DISCONNECT) {
			log.warn("Client packet decryption failed " + std::to_string(corruptPackets.get()) + " times, disconnecting " + toString());
			return false;
		}
		if (log.isDebugEnabled())
			log.debug("[" + std::to_string(corruptPackets.get()) + "/" + std::to_string(MAX_CORRUPT_PACKETS_BEFORE_DISCONNECT) +
				"] Decrypt fail, client packet passed...");
		return true;
	}

	if (data.remaining() < 5) { // op + static code + op == 5 bytes
		log.warn("Received fake packet from " + toString() + ", disconnecting");
		return false;
	}

	std::unique_ptr<AionClientPacket> pck = AionClientPacketFactory::tryCreatePacket(data, this);

	// Execute packet only if packet exist (!= null) and read was ok.
	if (pck) {
		lastClientMessageTime.set(commons::utils::currentTimeMillis());
		// C++: pffRequests always exists; Java creates it only with PFF_MODE > 0 (and a threshold map) when the connection is created
		if (PffConfig::PFF_MODE.load() > 0) {
			int32_t msBetweenPackets = PffConfig::getAllowedMillisBetweenPackets(*pck);
			if (msBetweenPackets > 0) {
				int64_t now = lastClientMessageTime.get();
				std::optional<int64_t> last = pffRequests.put(pck->getOpCode(), now);
				if (last) {
					int64_t diff = now - *last;
					if (diff < msBetweenPackets) {
						log.warn(toString() + " is flooding " + pck->getPacketName() + " (last diff: " + std::to_string(diff) + "ms)");
						if (PffConfig::PFF_MODE.load() == 1) // disconnect
							return false;
					}
				}
			}
		}

		if (pck->read()) {
			sendPacketInfo(*pck);
			packetProcessor().executePacket(std::move(pck));
		}
	}

	return true;
}

// lint: L7 Java synchronized (guard): AConnectionBase::doWrite calls writeData with guard held
bool AionConnection::writeData(commons::utils::ByteBuffer& data) {
	if (sendMsgQueue.empty())
		return false;
	SerializedBody body = std::move(sendMsgQueue.front());
	sendMsgQueue.pop_front();
	const std::vector<uint8_t>& bytes = *body.bytes;
	data.putShort(static_cast<int16_t>(bytes.size() + 2));
	data.put(std::span<const uint8_t>(bytes));
	data.flip();
	std::span<uint8_t> frame = data.remainingSpan();
	crypt.encrypt(frame.subspan(2)); // everything after the length; the first call (SM_KEY) only enables the crypt
	return true;
}

bool AionConnection::canReceivePacketInfoInChat() {
	if (getState() != State::IN_GAME)
		return false;
	runtime::Ptr<model::account::Account> acc = getAccount();
	return acc->getMembership() == 10;
}

void AionConnection::sendPacketInfo(commons::network::packet::BasePacket& packet) {
	if (canReceivePacketInfoInChat())
		sendPacket(serverpackets::SM_MESSAGE(0, "", packet.toFormattedPacketNameString(), model::ChatType::BRIGHT_YELLOW));
}

void AionConnection::sendUnknownClientPacketInfo(int32_t opCode) {
	if (canReceivePacketInfoInChat())
		sendPacket(serverpackets::SM_MESSAGE(0, "", commons::network::packet::BasePacket::toFormattedPacketNameString(3, opCode, "CM_UNK"),
			model::ChatType::YELLOW));
}

void AionConnection::onDisconnect() {
	runtime::TaskScope scope(runtime::TaskInfo{std::source_location::current(), runtime::TaskKind::INSTANT});
	if (runtime::Ptr<ConnectionAliveChecker> checker = connectionAliveChecker.get())
		checker->stop(); // C++: null if initialized() failed before creating it
	if (isGameServerShuttingDownSoon()) { // client crashing during last seconds of countdown
		safeLogout(); // instant synchronized leaveWorld to ensure completion before onServerClose
		return;
	}

	loginserver::LoginServer::getInstance().onDisconnect(this);

	runtime::Ptr<model::account::Account> acc = getAccount();
	std::string msg = !acc ? "" : " " + acc->toString();
	runtime::Ptr<Player> player = getActivePlayer();
	if (player) {
		msg += " " + player->toString() + " (client crash or connection loss)";
		player->getMoveController()->resetToLastPositionFromClient(); // avoid mapkick and bugging through walls
		int64_t millisSinceLastClientPacket = commons::utils::currentTimeMillis() - lastClientMessageTime.get();
		int64_t delayMs = std::max<int64_t>(0, std::chrono::milliseconds(std::chrono::seconds(10)).count() - millisSinceLastClientPacket);
		services::player::PlayerLeaveWorldService::leaveWorldDelayed(*player, delayMs); // delayed to prevent ctrl+alt+del / close window exploit
	}

	if (msg.empty())
		msg = " " + toString();

	log.info("Client disconnected:" + msg);
}

void AionConnection::onServerClose() {
	runtime::TaskScope scope(runtime::TaskInfo{std::source_location::current(), runtime::TaskKind::SHUTDOWN});
	close();
	safeLogout();
}

void AionConnection::safeLogout() {
	SYNCHRONIZED(*this) {
		runtime::Ptr<Player> player = getActivePlayer();
		if (!player) // player was already saved
			return;
		try {
			services::player::PlayerLeaveWorldService::leaveWorld(*player);
		} catch (...) {
			try {
				log.errorCurrentException("Error saving " + player->toString());
			} catch (...) {
				log.errorCurrentException("Error saving player");
			}
		}
	}
}

void AionConnection::encrypt(commons::utils::ByteBuffer& buf) {
	crypt.encrypt(buf.remainingSpan());
}

runtime::Ptr<model::account::Account> AionConnection::getAccount() {
	return account.get();
}

void AionConnection::setAccount(model::account::Account& value) {
	account.set(runtime::Ref<model::account::Account>(value)); // Java: Objects.requireNonNull(account, "Account can't be null")
}

bool AionConnection::setActivePlayer(runtime::Ptr<model::gameobjects::player::Player> player) {
	if (!player) {
		activePlayer.set(nullptr);
		setState(State::AUTHED);
	} else if (activePlayer.compareAndSet(nullptr, runtime::Ref<Player>(player))) {
		setState(State::IN_GAME);
	} else {
		return false;
	}
	return true;
}

runtime::Ptr<model::gameobjects::player::Player> AionConnection::getActivePlayer() {
	return activePlayer.get();
}

int32_t AionConnection::increaseAndGetPingFailCount() {
	return ++pingFailCount;
}

std::string AionConnection::toString() const {
	// C++: may be called from log statements outside a task (commons network code), so it opens its own scope for the field loads
	runtime::TaskScope scope(runtime::TaskInfo{std::source_location::current(), runtime::TaskKind::IO});
	auto* self = const_cast<AionConnection*>(this); // Java toString calls the non-const getters
	runtime::Ptr<model::account::Account> acc = self->getAccount();
	runtime::Ptr<Player> player = self->activePlayer.get();
	return "AionConnection [state=" + std::string(xml::enumName(state.get())) + ", account=" + (acc ? acc->toString() : std::string("null")) +
		", activePlayer=" + (player ? player->toString() : std::string("null")) + ", macAddress=" +
		(macAddress.get().empty() ? std::string("null") : macAddress.get()) + ", getIP()=" + getIP() + "]";
}

} // namespace aion::gameserver::network::aion
