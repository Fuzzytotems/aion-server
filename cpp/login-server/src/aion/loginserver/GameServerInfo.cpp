#include "aion/loginserver/GameServerInfo.h"

#include <utility>

#include "aion/commons/utils/Exception.h"
#include "aion/loginserver/model/Account.h"
#include "aion/loginserver/network/gameserver/GsConnection.h"

namespace aion::loginserver {

using network::gameserver::GsConnection;

GameServerInfo::GameServerInfo(int8_t id, std::string ipMask, std::string password) : id(id), ipMask(std::move(ipMask)), password(std::move(password)) {}

int32_t GameServerInfo::getPort() const {
	std::lock_guard lock(mutex);
	return port;
}

void GameServerInfo::setPort(int32_t value) {
	std::lock_guard lock(mutex);
	port = value;
}

std::vector<uint8_t> GameServerInfo::getIp() const {
	std::lock_guard lock(mutex);
	return ip;
}

void GameServerInfo::setIp(std::vector<uint8_t> value) {
	std::lock_guard lock(mutex);
	ip = std::move(value);
}

std::shared_ptr<GsConnection> GameServerInfo::getConnection() const {
	std::lock_guard lock(mutex);
	return gscHandler;
}

void GameServerInfo::setConnection(std::shared_ptr<GsConnection> gsConnection) {
	std::shared_ptr<GsConnection> old;
	std::lock_guard lock(mutex);
	old = std::exchange(gscHandler, std::move(gsConnection)); // destroyed after the lock is released (declared before the lock)
}

void GameServerInfo::removeConnection(const GsConnection& gsConnection) {
	// destroyed after the lock is released (declared before the lock)
	std::shared_ptr<GsConnection> old;
	std::unordered_map<int32_t, std::shared_ptr<model::Account>> cleared;
	std::lock_guard lock(mutex);
	if (gscHandler.get() == &gsConnection) {
		old = std::exchange(gscHandler, nullptr);
		cleared.swap(accountsOnGameServer);
	}
}

int8_t GameServerInfo::getMinAccessLevel() const {
	std::lock_guard lock(mutex);
	return minAccessLevel;
}

void GameServerInfo::setMinAccessLevel(int8_t value) {
	std::lock_guard lock(mutex);
	minAccessLevel = value;
}

int32_t GameServerInfo::getMaxPlayers() const {
	std::lock_guard lock(mutex);
	return maxPlayers;
}

void GameServerInfo::setMaxPlayers(int32_t value) {
	std::lock_guard lock(mutex);
	maxPlayers = value;
}

bool GameServerInfo::isOnline() const {
	std::shared_ptr<GsConnection> connection = getConnection();
	return connection && connection->getState() == GsConnection::State::AUTHED;
}

bool GameServerInfo::isAccountOnGameServer(int32_t accountId) const {
	std::lock_guard lock(mutex);
	return accountsOnGameServer.contains(accountId);
}

std::shared_ptr<model::Account> GameServerInfo::removeAccountFromGameServer(int32_t accountId) {
	std::lock_guard lock(mutex);
	auto it = accountsOnGameServer.find(accountId);
	if (it == accountsOnGameServer.end())
		return nullptr;
	std::shared_ptr<model::Account> account = std::move(it->second);
	accountsOnGameServer.erase(it);
	return account;
}

bool GameServerInfo::addAccountToGameServer(std::shared_ptr<model::Account> acc, const GsConnection& gsConnection) {
	if (!acc)
		throw commons::utils::IllegalArgumentException("Cannot add a null account to game server #" + std::to_string(id));
	int32_t accountId = acc->getId().value();
	std::shared_ptr<model::Account> replaced;
	std::lock_guard lock(mutex);
	if (gscHandler.get() != &gsConnection)
		return false;
	replaced = std::exchange(accountsOnGameServer[accountId], std::move(acc));
	return true;
}

std::shared_ptr<model::Account> GameServerInfo::getAccountFromGameServer(int32_t accountId) const {
	std::lock_guard lock(mutex);
	auto it = accountsOnGameServer.find(accountId);
	return it == accountsOnGameServer.end() ? nullptr : it->second;
}

void GameServerInfo::clearAccountsOnGameServer() {
	std::unordered_map<int32_t, std::shared_ptr<model::Account>> cleared;
	std::lock_guard lock(mutex);
	cleared.swap(accountsOnGameServer);
}

int32_t GameServerInfo::getCurrentPlayers() const {
	std::lock_guard lock(mutex);
	return static_cast<int32_t>(accountsOnGameServer.size());
}

bool GameServerInfo::isFull() const {
	std::lock_guard lock(mutex);
	return static_cast<int32_t>(accountsOnGameServer.size()) >= maxPlayers;
}

} // namespace aion::loginserver
