#include "aion/gameserver/services/GameTimeService.h"

#include <any>
#include <string>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/dao/ServerVariablesDAO.h"
#include "aion/gameserver/network/aion/serverpackets/SM_GAME_TIME.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"
#include "aion/gameserver/utils/time/gametime/GameTime.h"

namespace aion::gameserver::services {

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.services.GameTimeService");

GameTimeService::GameTimeService() : gameTime(utils::time::gametime::GameTime::create(dao::ServerVariablesDAO::loadInt("time"))) {
	log.info("Initialized GameTime");
}

GameTimeService::~GameTimeService() = default;

GameTimeService& GameTimeService::getInstance() {
	static GameTimeService instance; // Java SingletonHolder
	return instance;
}

bool GameTimeService::saveGameTime() {
	return dao::ServerVariablesDAO::store("time", std::any(gameTime->getTime()));
}

void GameTimeService::startClock() {
	if (!isStarted.compareAndSet(false, true))
		throw runtime::Exception("Tried to start game time twice."); // Java: GameServerError (P5-14 has no C++ class; CONVENTIONS: fatal errors)

	int32_t updateInterval = 3 * 60000; // every 3 minutes

	// task to increase the game time every 5 seconds by a minute
	// callback at GameTimeService.java:51 (fieldmap key GameTimeService@L51:55): pin {this}
	utils::ThreadPoolManager::getInstance().scheduleAtFixedRate({this}, [this] { gameTime->addMinutes(1); }, 5000, 5000);

	// task to save the game time and update all clients
	// callback at GameTimeService.java:54 (fieldmap key GameTimeService@L54:55): pin {this}
	utils::ThreadPoolManager::getInstance().scheduleAtFixedRate({this}, [this] {
		log.info("Sending current game time to all players");
		utils::PacketSendUtility::broadcastToWorld(network::aion::serverpackets::SM_GAME_TIME());
		if (saveGameTime())
			log.info("Game time saved...");
		else
			log.warn("Error saving game time");
	}, updateInterval, updateInterval);

	log.info("GameTime started. Update interval: " + std::to_string(updateInterval / 1000) + "s");
}

} // namespace aion::gameserver::services
