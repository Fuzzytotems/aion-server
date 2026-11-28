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
		throw runtime::IllegalStateException("Tried to start game time twice.");

	int32_t updateInterval = 3 * 60000; // every 3 minutes

	// task to increase the game time every 5 seconds by a minute
	// callback at GameTimeService.java:51 (fieldmap key GameTimeService@L51:55): pin {this}
	utils::ThreadPoolManager::getInstance().scheduleAtFixedRate({this}, [this] { gameTime->addMinutes(1); }, 5000, 5000);

	// task to save the game time and update all clients
	// callback at GameTimeService.java:54 (fieldmap key GameTimeService@L54:55): pin {this}
	utils::ThreadPoolManager::getInstance().scheduleAtFixedRate({this}, [this] {
		log.info("Saving current game time and sending it to all players");
		if (!saveGameTime())
			log.warn("Error saving game time");
		utils::PacketSendUtility::broadcastToWorld(network::aion::serverpackets::SM_GAME_TIME());
	}, updateInterval, updateInterval);

	log.info("Game time started with an update interval of {} seconds", updateInterval / 1000);
}

} // namespace aion::gameserver::services
