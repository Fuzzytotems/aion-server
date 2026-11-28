#include "aion/loginserver/network/aion/LoginConnection.h"

#include <limits>
#include <utility>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/utils/Rnd.h"
#include "aion/loginserver/configs/Config.h"
#include "aion/loginserver/controller/AccountController.h"
#include "aion/loginserver/controller/AccountTimeController.h"
#include "aion/loginserver/model/Account.h"
#include "aion/loginserver/network/aion/AionClientPacket.h"
#include "aion/loginserver/network/aion/serverpackets/SM_INIT.h"
#include "aion/loginserver/network/factories/AionPacketHandlerFactory.h"
#include "aion/loginserver/network/ncrypt/KeyGen.h"

namespace aion::loginserver::network::aion {

namespace {

const commons::logging::Logger& log() {
	static const auto* logger = new commons::logging::Logger(commons::logging::LoggerFactory::getLogger("com.aionemu.loginserver.network.aion.LoginConnection"));
	return *logger;
}

} // namespace

LoginConnection::LoginConnection(std::shared_ptr<commons::network::netty::ConnectionHandler> channel, Executor packetExecutor)
	: NettyConnection(std::move(channel), std::move(packetExecutor)),
		sessionId(commons::utils::Rnd::get(1, std::numeric_limits<int32_t>::max())) {}

std::unique_ptr<commons::network::packet::ClientPacketBase> LoginConnection::createPacket(commons::utils::ByteBuffer& data) {
	if (!cryptEngine.decrypt(data.remainingSpan())) {
		log().warn("Wrong checksum from {}", toString());
		close();
		return nullptr;
	}
	data.limit(data.limit() - 8); // the checksum word and the unused word after it
	return factories::AionPacketHandlerFactory::handle(data, std::static_pointer_cast<LoginConnection>(shared_from_this()));
}

void LoginConnection::writePacket(const AionServerPacket& packet, commons::utils::ByteBuffer& buffer) {
	packet.write(*this, buffer);
}

void LoginConnection::onDisconnect() {
	Logout claimed;
	{
		std::lock_guard lock(fieldMutex);
		disconnected = true;
		if (accountAttachInProgress)
			return; // the AccountAttachScope logs out when the packet finished
		claimed = claimLogout();
	}
	logout(claimed);
}

LoginConnection::Logout LoginConnection::claimLogout() {
	Logout claimed;
	// Remove account only if not joined GameServer yet.
	if (account && !isJoinedGs()) {
		claimed.account = account;
		claimed.updateTime = account != loggedOutAccount;
		loggedOutAccount = account;
	}
	return claimed;
}

void LoginConnection::logout(const Logout& claimed) {
	if (!claimed.account)
		return;
	controller::AccountController::removeAccountOnLS(*claimed.account, *this);
	if (claimed.updateTime)
		controller::AccountTimeController::updateOnLogout(*claimed.account);
}

LoginConnection::AccountAttachScope::AccountAttachScope(LoginConnection& attachingConnection) : connection(attachingConnection) {
	std::lock_guard lock(connection.fieldMutex);
	connection.accountAttachInProgress = true;
}

LoginConnection::AccountAttachScope::~AccountAttachScope() {
	try {
		Logout claimed;
		{
			std::lock_guard lock(connection.fieldMutex);
			connection.accountAttachInProgress = false;
			if (!connection.disconnected)
				return; // onDisconnect will log out
			claimed = connection.claimLogout();
		}
		connection.logout(claimed);
	} catch (...) {
		try {
			log().errorCurrentException("Could not log out the account of disconnected " + connection.toString());
		} catch (...) {
			// nothing may escape the destructor
		}
	}
}

int64_t LoginConnection::getAuthTimeoutMillis() const {
	return configs::Config::CLIENT_AUTH_TIMEOUT_SECONDS * 1000LL;
}

int32_t LoginConnection::encrypt(std::span<uint8_t> data, int32_t length) {
	return cryptEngine.encrypt(data, length);
}

std::span<const uint8_t> LoginConnection::getEncryptedModulus() const {
	return encryptedRSAKeyPair ? encryptedRSAKeyPair->getEncryptedModulus() : std::span<const uint8_t>();
}

std::shared_ptr<model::Account> LoginConnection::getAccount() const {
	std::lock_guard lock(fieldMutex);
	return account;
}

void LoginConnection::setAccount(std::shared_ptr<model::Account> newAccount) {
	std::lock_guard lock(fieldMutex);
	account = std::move(newAccount);
}

std::optional<SessionKey> LoginConnection::getSessionKey() const {
	std::lock_guard lock(fieldMutex);
	return sessionKey;
}

void LoginConnection::setSessionKey(const SessionKey& newSessionKey) {
	std::lock_guard lock(fieldMutex);
	sessionKey = newSessionKey;
}

std::string LoginConnection::toString() const {
	std::shared_ptr<model::Account> acc = getAccount();
	return (acc ? acc->toString() + " " : std::string("Client ")) + getIP();
}

void LoginConnection::onConnect() {
	log().info("Connection attempt from: {}", getIP());
	encryptedRSAKeyPair = ncrypt::KeyGen::getEncryptedRSAKeyPair();
	const ncrypt::KeyGen::BlowfishKey blowfishKey = ncrypt::KeyGen::generateBlowfishKey();

	cryptEngine.updateKey(blowfishKey);

	sendPacket(serverpackets::SM_INIT(*this, blowfishKey));
}

const char* toString(LoginConnection::State state) noexcept {
	switch (state) {
		case LoginConnection::State::CONNECTED:
			return "CONNECTED";
		case LoginConnection::State::AUTHED_GG:
			return "AUTHED_GG";
		case LoginConnection::State::AUTHED_LOGIN:
			return "AUTHED_LOGIN";
	}
	return "UNKNOWN";
}

} // namespace aion::loginserver::network::aion
