#include "aion/loginserver/GameServerTable.h"

#include <map>
#include <mutex>
#include <utility>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/utils/NetworkUtils.h"
#include "aion/loginserver/dao/GameServersDAO.h"
#include "aion/loginserver/network/gameserver/GsConnection.h"
#include "aion/loginserver/network/gameserver/serverpackets/SM_REQUEST_KICK_ACCOUNT.h"

namespace aion::loginserver::GameServerTable {

using network::gameserver::GsAuthResponse;
using network::gameserver::GsConnection;

namespace {

using GameServers = std::map<int8_t, std::shared_ptr<GameServerInfo>>;

const commons::logging::Logger& log() {
	static const auto* logger = new commons::logging::Logger(commons::logging::LoggerFactory::getLogger("com.aionemu.loginserver.GameServerTable"));
	return *logger;
}

struct State {
	/** guards gameservers (leaf lock) */
	std::mutex mutex;
	/** Map<Id,GameServer>, replaced as a whole by load() */
	std::shared_ptr<const GameServers> gameservers = std::make_shared<const GameServers>();
	/** serializes registerGameServer */
	std::mutex registerMutex;
};

// leaked: game server infos may be referenced by connections that are destroyed during static destruction
State& state() {
	static auto* s = new State();
	return *s;
}

std::shared_ptr<const GameServers> snapshot() {
	std::lock_guard lock(state().mutex);
	return state().gameservers;
}

} // namespace

std::vector<std::shared_ptr<GameServerInfo>> getGameServers() {
	std::shared_ptr<const GameServers> gameservers = snapshot();
	std::vector<std::shared_ptr<GameServerInfo>> result;
	result.reserve(gameservers->size());
	for (const auto& [id, gsi] : *gameservers)
		result.push_back(gsi);
	return result;
}

int32_t size() {
	return static_cast<int32_t>(snapshot()->size());
}

void load() {
	auto loaded = std::make_shared<GameServers>();
	for (auto& [id, gsi] : dao::GameServersDAO::getAllGameServers<GameServerInfo>())
		loaded->emplace(id, std::move(gsi));
	std::shared_ptr<const GameServers> old;
	{
		std::lock_guard lock(state().mutex);
		old = std::exchange(state().gameservers, std::move(loaded));
	}
	log().info("GameServerTable loaded " + std::to_string(size()) + " registered GameServers.");
}

GsAuthResponse registerGameServer(const std::shared_ptr<GsConnection>& gsConnection, int8_t requestedId, std::string_view password,
	std::vector<uint8_t> ip, int32_t port, int8_t minAccessLevel, int32_t maxPlayers) {
	std::shared_ptr<GameServerInfo> gsi = getGameServerInfo(requestedId);

	// This id is not Registered at LoginServer.
	if (!gsi) {
		log().warn(gsConnection->toString() + " requestedID: " + std::to_string(requestedId) + " is not registered in LS database!");
		return GsAuthResponse::NOT_AUTHED;
	}

	// Deviation: registrations are serialized, so two game servers requesting the same id at once cannot both be registered (Java: data race)
	std::lock_guard lock(state().registerMutex);

	// Check if this GameServer is not already registered.
	if (gsi->getConnection())
		return GsAuthResponse::ALREADY_REGISTERED;

	// Check if password and ip are ok.
	if (gsi->getPassword() != password || !commons::utils::NetworkUtils::checkIPMatching(gsi->getIpMask(), gsConnection->getIP())) {
		log().warn(gsConnection->toString() + " requested ID: " + std::to_string(requestedId) + " has wrong IP or password!");
		return GsAuthResponse::NOT_AUTHED;
	}

	gsi->setIp(std::move(ip));
	gsi->setPort(port);
	gsi->setMinAccessLevel(minAccessLevel);
	gsi->setMaxPlayers(maxPlayers);
	gsi->setConnection(gsConnection);

	gsConnection->setGameServerInfo(gsi);
	// Deviation: if the game server disconnected meanwhile, its onDisconnect may have missed the GameServerInfo (Java: the id stays registered
	// with a dead connection forever); unregister it again
	if (gsConnection->isClosed()) {
		gsi->removeConnection(*gsConnection);
		gsConnection->setGameServerInfo(nullptr);
	}
	return GsAuthResponse::AUTHED;
}

std::shared_ptr<GameServerInfo> getGameServerInfo(int8_t gameServerId) {
	std::shared_ptr<const GameServers> gameservers = snapshot();
	auto it = gameservers->find(gameServerId);
	return it == gameservers->end() ? nullptr : it->second;
}

std::shared_ptr<GameServerInfo> findLoggedInAccountGs(int32_t accountId) {
	for (const std::shared_ptr<GameServerInfo>& gsi : getGameServers()) {
		if (gsi->isAccountOnGameServer(accountId))
			return gsi;
	}
	return nullptr;
}

bool kickAccountFromGameServer(int32_t accountId, bool notifyDoubleLogin) {
	std::shared_ptr<GameServerInfo> gsi = findLoggedInAccountGs(accountId);
	if (!gsi)
		return false;
	// Deviation: the connection may have been cleared by a concurrent disconnect (Java: NullPointerException); the kick request is skipped then
	if (std::shared_ptr<GsConnection> connection = gsi->getConnection())
		connection->sendPacket(std::make_shared<network::gameserver::serverpackets::SM_REQUEST_KICK_ACCOUNT>(accountId, notifyDoubleLogin));
	return true;
}

} // namespace aion::loginserver::GameServerTable
