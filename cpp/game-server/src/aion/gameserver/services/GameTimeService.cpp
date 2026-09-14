#include "aion/gameserver/services/GameTimeService.h"

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/utils/time/gametime/GameTime.h"

namespace aion::gameserver::services {

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.services.GameTimeService");

// Java field initializer gameTime = new GameTime(ServerVariablesDAO.loadInt("time")) and log.info: unported (DAO)
GameTimeService::GameTimeService() : gameTime() {
	AION_UNPORTED();
}

GameTimeService::~GameTimeService() = default;

GameTimeService& GameTimeService::getInstance() {
	static GameTimeService instance; // Java SingletonHolder
	return instance;
}

bool GameTimeService::saveGameTime() {
	AION_UNPORTED();
}

// callback at GameTimeService.java:51 (fieldmap key GameTimeService@L51:55)
// callback at GameTimeService.java:54 (fieldmap key GameTimeService@L54:55)
void GameTimeService::startClock() {
	AION_UNPORTED();
}

} // namespace aion::gameserver::services
