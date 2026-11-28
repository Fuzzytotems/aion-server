#include "aion/chatserver/service/GameServerService.h"

#include "aion/chatserver/configs/network/NetworkConfig.h"
#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/utils/MessageDigest.h"

namespace aion::chatserver::service {

using network::gameserver::GsAuthResponse;

namespace {

const commons::logging::Logger& log() {
	static const auto* logger = new commons::logging::Logger(commons::logging::LoggerFactory::getLogger("com.aionemu.chatserver.service.GameServerService"));
	return *logger;
}

} // namespace

GameServerService& GameServerService::getInstance() {
	static auto* instance = new GameServerService(); // leaked: connections may disconnect while static objects are destroyed
	return *instance;
}

GsAuthResponse GameServerService::registerGameServer(int8_t gameServerId, std::string_view password, bool fromLoopback) {
	std::lock_guard lock(mutex);
	if (isOnline)
		return GsAuthResponse::ALREADY_REGISTERED;
	if (configs::network::NetworkConfig::GAMESERVER_PASSWORD.empty() && !fromLoopback) {
		log().warn("Rejected game server #{} from a remote address: chatserver.network.gameserver.password is not set", gameServerId);
		return GsAuthResponse::NOT_AUTHED;
	}
	if (!commons::utils::MessageDigest::isEqual(password, configs::network::NetworkConfig::GAMESERVER_PASSWORD))
		return GsAuthResponse::NOT_AUTHED;
	isOnline = true;
	GAMESERVER_ID = gameServerId;
	return GsAuthResponse::AUTHED;
}

void GameServerService::setOffline() {
	log().info("Game server #{} disconnected", GAMESERVER_ID.load());
	std::lock_guard lock(mutex);
	isOnline = false;
}

} // namespace aion::chatserver::service
