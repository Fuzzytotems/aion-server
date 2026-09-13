#include "aion/loginserver/controller/AccountController.h"

#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "aion/commons/utils/Exception.h"
#include "aion/commons/utils/NetworkUtils.h"
#include "aion/loginserver/GameServerInfo.h"
#include "aion/loginserver/GameServerTable.h"
#include "aion/loginserver/configs/Config.h"
#include "aion/loginserver/controller/AccountTimeController.h"
#include "aion/loginserver/controller/BannedIpController.h"
#include "aion/loginserver/dao/AccountDAO.h"
#include "aion/loginserver/dao/AccountTimeDAO.h"
#include "aion/loginserver/model/Account.h"
#include "aion/loginserver/network/aion/LoginConnection.h"
#include "aion/loginserver/network/aion/serverpackets/SM_ACCOUNT_BANNED_2.h"
#include "aion/loginserver/network/aion/serverpackets/SM_ACCOUNT_KICK.h"
#include "aion/loginserver/network/aion/serverpackets/SM_SERVER_LIST.h"
#include "aion/loginserver/network/aion/serverpackets/SM_UPDATE_SESSION.h"
#include "aion/loginserver/network/gameserver/GsConnection.h"
#include "aion/loginserver/network/gameserver/serverpackets/SM_ACCOUNT_AUTH_RESPONSE.h"
#include "aion/loginserver/network/gameserver/serverpackets/SM_GS_CHARACTER_RESPONSE.h"
#include "aion/loginserver/utils/AccountUtils.h"
#include "aion/loginserver/utils/ExternalAuth.h"

namespace aion::loginserver::controller::AccountController {

using configs::Config;
using model::Account;
using model::ReconnectingAccount;
using network::aion::AionAuthResponse;
using network::aion::LoginConnection;
using network::aion::SessionKey;
using network::gameserver::GsConnection;
using namespace network::aion::serverpackets;
using namespace network::gameserver::serverpackets;

namespace {

using CharacterCounts = std::map<int8_t, int32_t>;

struct State {
	/** Java: synchronized (AccountController.class) */
	std::recursive_mutex controllerLock;

	/** guards accountsOnLS (leaf lock) */
	std::mutex accountsOnLSLock;
	/** Map with accounts that are active on LoginServer or joined GameServer and are not authenticated yet. */
	std::unordered_map<int32_t, std::shared_ptr<LoginConnection>> accountsOnLS;

	/** Map with accounts that are reconnecting to LoginServer ie was joined GameServer. Guarded by controllerLock. */
	std::unordered_map<int32_t, ReconnectingAccount> reconnectingAccounts;

	/** guards accountsGSCharacterCounts (leaf lock) */
	std::mutex characterCountsLock;
	/** Map with characters count on each gameserver and accounts */
	std::unordered_map<int32_t, CharacterCounts> accountsGSCharacterCounts;
};

// leaked: connections kept in the maps may outlive the network and must not be destroyed during static destruction
State& state() {
	static auto* s = new State();
	return *s;
}

std::shared_ptr<LoginConnection> getConnectionOnLS(int32_t accountId) {
	std::lock_guard lock(state().accountsOnLSLock);
	auto it = state().accountsOnLS.find(accountId);
	return it == state().accountsOnLS.end() ? nullptr : it->second;
}

std::shared_ptr<LoginConnection> removeAccountOnLS(int32_t accountId) {
	std::lock_guard lock(state().accountsOnLSLock);
	auto it = state().accountsOnLS.find(accountId);
	if (it == state().accountsOnLS.end())
		return nullptr;
	std::shared_ptr<LoginConnection> con = std::move(it->second);
	state().accountsOnLS.erase(it);
	return con;
}

void putAccountOnLS(int32_t accountId, std::shared_ptr<LoginConnection> connection) {
	std::shared_ptr<LoginConnection> replaced;
	std::lock_guard lock(state().accountsOnLSLock);
	replaced = std::exchange(state().accountsOnLS[accountId], std::move(connection));
}

void setAccountTime(Account& account) {
	std::optional<model::AccountTime> accTime = dao::AccountTimeDAO::getAccountTime(account.getId().value());
	if (!accTime)
		throw commons::utils::IllegalStateException("Account Time for account " + account.toString() + " is null");
	account.setAccountTime(std::move(accTime));
}

} // namespace

void removeAccountOnLS(const Account& account) {
	removeAccountOnLS(account.getId().value());
}

void removeAccountOnLS(const Account& account, const LoginConnection& connection) {
	std::shared_ptr<LoginConnection> removed;
	std::lock_guard lock(state().accountsOnLSLock);
	auto it = state().accountsOnLS.find(account.getId().value());
	if (it != state().accountsOnLS.end() && it->second.get() == &connection) {
		removed = std::move(it->second);
		state().accountsOnLS.erase(it);
	}
}

std::shared_ptr<Account> getAccountOnLS(int32_t accountId) {
	std::shared_ptr<LoginConnection> con = getConnectionOnLS(accountId);
	return con ? con->getAccount() : nullptr;
}

void checkAuth(const SessionKey& key, const std::shared_ptr<GsConnection>& gsConnection) {
	std::shared_ptr<LoginConnection> con = getConnectionOnLS(key.accountId);
	std::optional<SessionKey> sessionKey = con ? con->getSessionKey() : std::nullopt;

	if (con && sessionKey && sessionKey->checkSessionKey(key)) {
		// account is successful logged in on gs remove it from here
		removeAccountOnLS(key.accountId);

		std::shared_ptr<GameServerInfo> gsi = gsConnection->getGameServerInfo();
		if (!gsi)
			throw commons::utils::IllegalStateException(gsConnection->toString() + " has no GameServerInfo");
		std::shared_ptr<Account> acc = con->getAccount();
		if (!acc)
			throw commons::utils::IllegalStateException(con->toString() + " has no account");

		// Add account to accounts on GameServer list and update accounts last server
		if (!gsi->addAccountToGameServer(acc, *gsConnection)) {
			// Deviation: the game server disconnected meanwhile (see GameServerInfo::addAccountToGameServer), so the account is neither on the login
			// server nor on a game server and may log in again; the answer is not delivered anymore
			gsConnection->sendPacket(std::make_shared<SM_ACCOUNT_AUTH_RESPONSE>(key.accountId, false, "", 0, int8_t{0}, int8_t{0}, std::nullopt, std::nullopt));
			return;
		}

		acc->setLastServer(gsi->getId());
		dao::AccountDAO::updateLastServer(acc->getId().value(), acc->getLastServer());

		// Send response to GameServer
		gsConnection->sendPacket(std::make_shared<SM_ACCOUNT_AUTH_RESPONSE>(key.accountId, true, acc->getName(),
			acc->getCreationDate().value().time_since_epoch().count(), acc->getAccessLevel(), acc->getMembership(), acc->getAllowedHddSerial(),
			acc->getAccountTime()));
	} else {
		gsConnection->sendPacket(std::make_shared<SM_ACCOUNT_AUTH_RESPONSE>(key.accountId, false, "", 0, int8_t{0}, int8_t{0}, std::nullopt, std::nullopt));
	}
}

void addReconnectingAccount(ReconnectingAccount acc) {
	std::lock_guard lock(state().controllerLock);
	int32_t accountId = acc.getAccount()->getId().value();
	state().reconnectingAccounts.insert_or_assign(accountId, std::move(acc));
}

std::shared_ptr<Account> getReconnectingAccount(int32_t accountId) {
	std::lock_guard lock(state().controllerLock);
	auto it = state().reconnectingAccounts.find(accountId);
	return it == state().reconnectingAccounts.end() ? nullptr : it->second.getAccount();
}

void authReconnectingAccount(int32_t accountId, int32_t loginOk, int32_t reconnectKey, const std::shared_ptr<LoginConnection>& client) {
	std::lock_guard lock(state().controllerLock);
	std::optional<ReconnectingAccount> reconnectingAccount;
	if (auto it = state().reconnectingAccounts.find(accountId); it != state().reconnectingAccounts.end()) {
		reconnectingAccount = std::move(it->second);
		state().reconnectingAccounts.erase(it);
	}

	if (reconnectingAccount && reconnectingAccount->getReconnectionKey() == reconnectKey) {
		std::shared_ptr<Account> acc = reconnectingAccount->getAccount();

		client->setAccount(acc);
		putAccountOnLS(acc->getId().value(), client);
		client->setState(LoginConnection::State::AUTHED_LOGIN);
		client->setSessionKey(SessionKey(*acc));
		client->sendPacket(std::make_shared<SM_UPDATE_SESSION>(client->getSessionKey().value()));
	} else {
		client->close();
	}
}

std::optional<AionAuthResponse> login(std::string_view name, std::string_view password, const std::shared_ptr<LoginConnection>& connection) {
	// if ip is banned
	if (BannedIpController::isBanned(connection->getIP())) {
		return AionAuthResponse::STR_L2AUTH_S_BLOCKED_IP;
	}

	std::optional<std::string> accountName = std::string(name);

	if (Config::useExternalAuth()) {
		std::optional<utils::ExternalAuth::Response> auth = utils::ExternalAuth::authenticate(name, password);
		if (!auth) {
			return AionAuthResponse::STR_L2AUTH_S_ACCOUNTCACHESERVER_DOWN;
		}

		AionAuthResponse response = network::aion::getByIdOrDefault(auth->aionAuthResponseId, AionAuthResponse::STR_L2AUTH_UNKNOWN4);
		if (response != AionAuthResponse::STR_L2AUTH_S_ALL_OK) {
			return response;
		}
		accountName = auth->accountId;
	}

	// Java: AccountDAO.getAccount(null) finds no account
	std::shared_ptr<Account> account = accountName ? loadAccount(*accountName) : nullptr;

	// Try to create new account
	if (!account && Config::ACCOUNT_AUTO_CREATION && accountName && !accountName->empty()) {
		account = createAccount(*accountName, password);
	}

	// if account not found and not created
	if (!account) {
		return AionAuthResponse::STR_L2AUTH_S_ACCOUNT_LOAD_FAIL;
	}

	// if not external authentication, verify password hash from database
	if (!Config::useExternalAuth() && account->getPasswordHash() != utils::AccountUtils::encodePassword(password)) {
		return AionAuthResponse::STR_L2AUTH_S_INCORRECT_PWD;
	}

	// if account is not activated
	if (account->getActivated() != 1) {
		return AionAuthResponse::STR_L2AUTH_S_AGREE_GAME;
	}

	// if account expired
	if (AccountTimeController::isAccountExpired(*account)) {
		return AionAuthResponse::STR_L2AUTH_S_TIME_EXHAUSTED;
	}

	// if account is banned
	if (AccountTimeController::isAccountPenaltyActive(*account)) {
		connection->close(std::make_shared<SM_ACCOUNT_BANNED_2>());
		return std::nullopt;
	}

	// if account is restricted to some ip or mask
	std::optional<std::string> ipForce = account->getIpForce();
	if (ipForce && !commons::utils::NetworkUtils::checkIPMatching(*ipForce, connection->getIP())) {
		return AionAuthResponse::STR_L2AUTH_S_BLOCKED_IP;
	}

	const int32_t accountId = account->getId().value();

	// Do not allow to login two times with same account
	{
		std::lock_guard lock(state().controllerLock);
		if (GameServerTable::kickAccountFromGameServer(accountId, true))
			return AionAuthResponse::STR_L2AUTH_S_ALREADY_LOGIN;

		// If someone is at loginserver, he should be disconnected
		std::shared_ptr<LoginConnection> con = removeAccountOnLS(accountId);
		if (con) {
			con->close(std::make_shared<SM_ACCOUNT_KICK>(AionAuthResponse::STR_L2AUTH_S_KICKED_DOUBLE_LOGIN));
			return AionAuthResponse::STR_L2AUTH_S_ALREADY_LOGIN;
		}
		connection->setAccount(account);
		putAccountOnLS(accountId, connection);
	}

	AccountTimeController::updateOnLogin(*account);

	// if everything was OK
	dao::AccountDAO::updateLastIp(accountId, connection->getIP());
	// last mac is updated after receiving packet from gameserver
	dao::AccountDAO::updateMembership(accountId);

	return AionAuthResponse::STR_L2AUTH_S_ALL_OK;
}

void kickAccount(int32_t accountId) {
	std::lock_guard lock(state().controllerLock);
	GameServerTable::kickAccountFromGameServer(accountId, false);
	state().reconnectingAccounts.erase(accountId); // Deviation, see the header

	std::shared_ptr<LoginConnection> conn = removeAccountOnLS(accountId);
	if (conn)
		conn->close(std::make_shared<SM_ACCOUNT_KICK>(AionAuthResponse::STR_L2AUTH_S_BLOCKED_IP));
}

std::shared_ptr<Account> loadAccount(std::string_view name) {
	std::shared_ptr<Account> account = dao::AccountDAO::getAccount(name);
	if (account)
		setAccountTime(*account);
	return account;
}

std::shared_ptr<Account> loadAccount(int32_t id) {
	std::shared_ptr<Account> account = dao::AccountDAO::getAccount(id);
	if (account)
		setAccountTime(*account);
	return account;
}

std::shared_ptr<Account> createAccount(std::string_view name, std::string_view password) {
	std::string passwordHash = Config::useExternalAuth() ? "" : utils::AccountUtils::encodePassword(password);
	auto account = std::make_shared<Account>();

	account->setName(std::string(name));
	account->setPasswordHash(std::move(passwordHash));
	account->setAccessLevel(0);
	account->setMembership(0);
	account->setActivated(1);

	if (dao::AccountDAO::insertAccount(*account)) {
		return account;
	}
	return nullptr;
}

void loadGSCharactersCount(int32_t accountId) {
	std::lock_guard lock(state().controllerLock);
	CharacterCounts accountCharacterCount;
	for (const std::shared_ptr<GameServerInfo>& gsi : GameServerTable::getGameServers()) {
		std::shared_ptr<GsConnection> gsc = gsi->getConnection();

		if (gsc)
			gsc->sendPacket(std::make_shared<SM_GS_CHARACTER_RESPONSE>(accountId));
		else
			accountCharacterCount.insert_or_assign(gsi->getId(), 0);
	}
	{
		std::lock_guard countsLock(state().characterCountsLock);
		state().accountsGSCharacterCounts.insert_or_assign(accountId, std::move(accountCharacterCount));
	}

	if (hasAllGSCharacterCounts(accountId))
		sendServerListFor(accountId);
}

bool hasAllGSCharacterCounts(int32_t accountId) {
	std::lock_guard lock(state().controllerLock);
	const int32_t gameServerCount = GameServerTable::size();
	std::lock_guard countsLock(state().characterCountsLock);
	auto characterCount = state().accountsGSCharacterCounts.find(accountId);
	return characterCount != state().accountsGSCharacterCounts.end() && static_cast<int32_t>(characterCount->second.size()) == gameServerCount;
}

void sendServerListFor(int32_t accountId) {
	std::shared_ptr<LoginConnection> con = getConnectionOnLS(accountId);
	if (con)
		con->sendPacket(std::make_shared<SM_SERVER_LIST>());
}

std::optional<std::map<int8_t, int32_t>> getGSCharacterCountsFor(int32_t accountId) {
	std::lock_guard lock(state().characterCountsLock);
	auto it = state().accountsGSCharacterCounts.find(accountId);
	if (it == state().accountsGSCharacterCounts.end())
		return std::nullopt;
	return it->second;
}

void addGSCharacterCountFor(int32_t accountId, int8_t gsid, int32_t characterCount) {
	std::lock_guard lock(state().controllerLock);
	std::lock_guard countsLock(state().characterCountsLock);
	state().accountsGSCharacterCounts[accountId].insert_or_assign(gsid, characterCount);
}

void updateServerListForAllLoggedInPlayers() {
	std::vector<std::shared_ptr<LoginConnection>> connections;
	{
		std::lock_guard lock(state().accountsOnLSLock);
		connections.reserve(state().accountsOnLS.size());
		for (const auto& [accountId, con] : state().accountsOnLS)
			connections.push_back(con);
	}
	for (const std::shared_ptr<LoginConnection>& con : connections) {
		if (con->getState() != LoginConnection::State::AUTHED_LOGIN || con->isJoinedGs())
			continue;
		std::shared_ptr<Account> account = con->getAccount();
		if (account && hasAllGSCharacterCounts(account->getId().value()))
			con->sendPacket(std::make_shared<SM_SERVER_LIST>());
	}
}

} // namespace aion::loginserver::controller::AccountController
