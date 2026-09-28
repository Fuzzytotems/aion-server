#include "aion/loginserver/network/aion/LoginConnection.h"

#include <limits>
#include <utility>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/utils/Rnd.h"
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

LoginConnection::LoginConnection(asio::ip::tcp::socket socket, commons::network::NioServer& server, std::shared_ptr<Processor> processor)
	: AConnection(std::move(socket), server, 8192 * 2, 8192 * 2),
		processor(std::move(processor)),
		sessionId(commons::utils::Rnd::get(1, std::numeric_limits<int32_t>::max())) {}

bool LoginConnection::processData(commons::utils::ByteBuffer& data) {
	if (!decrypt(data)) {
		log().warn("Wrong checksum from " + toString());
		return false;
	}

	std::unique_ptr<AionClientPacket> pck = factories::AionPacketHandlerFactory::handle(data, sharedFromThis());

	// Execute packet only if packet exists and read was ok.
	if (pck && pck->read())
		processor->executePacket(std::move(pck));

	return true;
}

bool LoginConnection::writeData(commons::utils::ByteBuffer& data) {
	// guard is held by the caller (Java: synchronized)
	if (sendMsgQueue.empty())
		return false;
	std::shared_ptr<AionServerPacket> packet = std::move(sendMsgQueue.front());
	sendMsgQueue.pop_front();
	packet->write(*this, data);
	return true;
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

void LoginConnection::onServerClose() {
	// TODO mb some packet should be send to client before closing?
	close(/* packet */);
}

bool LoginConnection::decrypt(commons::utils::ByteBuffer& buf) {
	return cryptEngine.decrypt(buf.remainingSpan());
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

void LoginConnection::initialized() {
	state = State::CONNECTED;
	log().info("Connection attempt from: " + getIP());
	encryptedRSAKeyPair = ncrypt::KeyGen::getEncryptedRSAKeyPair();
	const ncrypt::KeyGen::BlowfishKey blowfishKey = ncrypt::KeyGen::generateBlowfishKey();

	cryptEngine.updateKey(blowfishKey);

	sendPacket(std::make_shared<serverpackets::SM_INIT>(*this, blowfishKey));
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
