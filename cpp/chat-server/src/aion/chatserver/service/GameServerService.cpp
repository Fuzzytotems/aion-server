#include "aion/chatserver/service/GameServerService.h"

#include "aion/chatserver/configs/network/NetworkConfig.h"
#include "aion/commons/logging/LoggerFactory.h"

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

GsAuthResponse GameServerService::registerGameServer(int8_t gameServerId, std::string_view password) {
	std::lock_guard lock(mutex);
	if (isOnline)
		return GsAuthResponse::ALREADY_REGISTERED;
	if (password != configs::network::NetworkConfig::GAMESERVER_PASSWORD)
		return GsAuthResponse::NOT_AUTHED;
	isOnline = true;
	GAMESERVER_ID = gameServerId;
	return GsAuthResponse::AUTHED;
}

void GameServerService::setOffline() {
	log().info("Gameserver #{} is disconnected", GAMESERVER_ID.load());
	std::lock_guard lock(mutex);
	isOnline = false;
}

} // namespace aion::chatserver::service
