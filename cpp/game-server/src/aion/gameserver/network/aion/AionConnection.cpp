#include "aion/gameserver/network/aion/AionConnection.h"

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/runtime/base/Unported.h"

// S0b transition (docs/design/hub-headers.md §3.3): the constructor and the destructor instantiate the account and activePlayer references,
// which need the complete Account (not a hub; its header comes with P4-12) and Player (a hub of the objects group). Remove the guard once they
// exist.
#if __has_include("aion/gameserver/model/account/Account.h") && __has_include("aion/gameserver/model/gameobjects/player/Player.h")
#define AION_S0B_AION_CONNECTION_MEMBERS 1
#include "aion/gameserver/model/account/Account.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#else
#define AION_S0B_AION_CONNECTION_MEMBERS 0
#endif

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion {

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.network.aion.AionConnection");

runtime::Ref<AionConnection::ConnectionAliveChecker> AionConnection::ConnectionAliveChecker::create(
	std::weak_ptr<AionConnection> aionConnectionValue) {
	return runtime::makeRef<ConnectionAliveChecker>(std::move(aionConnectionValue));
}

AionConnection::ConnectionAliveChecker::ConnectionAliveChecker(std::weak_ptr<AionConnection> aionConnectionValue)
	: task(), aionConnection(std::move(aionConnectionValue)) {
	// Java: if (connectionAliveChecker != null) throw new IllegalStateException(...);
	// task = ThreadPoolManager.getInstance().scheduleAtFixedRate(this, CM_PING.CLIENT_PING_INTERVAL, CM_PING.CLIENT_PING_INTERVAL);
	AION_UNPORTED();
}

AionConnection::ConnectionAliveChecker::~ConnectionAliveChecker() = default;

void AionConnection::ConnectionAliveChecker::stop() {
	AION_UNPORTED();
}

void AionConnection::ConnectionAliveChecker::run() {
	AION_UNPORTED();
}

commons::network::PacketProcessor<AionConnection>& AionConnection::packetProcessor() {
	AION_UNPORTED();
}

#if AION_S0B_AION_CONNECTION_MEMBERS
AionConnection::AionConnection(asio::ip::tcp::socket socket, commons::network::NioServer& server)
	: AConnection(std::move(socket), server, 8192 * 4, 8192 * 4) {
	// Java: state = State.CONNECTED; log.debug("connection from: " + ip); lastClientMessageTime = System.currentTimeMillis();
	// connectionAliveChecker = new ConnectionAliveChecker(); (C++: in initialized()); pffRequests only with PffConfig.PFF_MODE > 0
	AION_UNPORTED();
}

AionConnection::~AionConnection() = default;
#endif

void AionConnection::sendPacket(AionServerPacket& packet) {
	AION_UNPORTED();
}

void AionConnection::enqueue(SerializedBody body) {
	AION_UNPORTED();
}

void AionConnection::close(AionServerPacket& closePacket) {
	AION_UNPORTED();
}

void AionConnection::initialized() {
	AION_UNPORTED();
}

int32_t AionConnection::enableCryptKey() {
	AION_UNPORTED();
}

bool AionConnection::processData(commons::utils::ByteBuffer& data) {
	AION_UNPORTED();
}

// lint: L7 Java synchronized (guard) block; AConnectionBase calls writeData with guard already held (commons AConnection)
bool AionConnection::writeData(commons::utils::ByteBuffer& data) {
	AION_UNPORTED();
}

bool AionConnection::canReceivePacketInfoInChat() {
	AION_UNPORTED();
}

void AionConnection::sendPacketInfo(commons::network::packet::BasePacket& packet) {
	AION_UNPORTED();
}

void AionConnection::sendUnknownClientPacketInfo(int32_t opCode) {
	AION_UNPORTED();
}

void AionConnection::onDisconnect() {
	AION_UNPORTED();
}

void AionConnection::onServerClose() {
	AION_UNPORTED();
}

// lint: L7 Java synchronized (this) block; the ported body keeps it as SYNCHRONIZED(*this) { ... } (hub-headers.md §11.4)
void AionConnection::safeLogout() {
	AION_UNPORTED();
}

void AionConnection::encrypt(commons::utils::ByteBuffer& buf) {
	AION_UNPORTED();
}

runtime::Ptr<model::account::Account> AionConnection::getAccount() {
	AION_UNPORTED();
}

void AionConnection::setAccount(model::account::Account& value) {
	AION_UNPORTED();
}

bool AionConnection::setActivePlayer(runtime::Ptr<model::gameobjects::player::Player> player) {
	AION_UNPORTED();
}

runtime::Ptr<model::gameobjects::player::Player> AionConnection::getActivePlayer() {
	AION_UNPORTED();
}

int32_t AionConnection::increaseAndGetPingFailCount() {
	AION_UNPORTED();
}

std::string AionConnection::toString() const {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion
