#include "aion/gameserver/services/TownService.h"

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/model/town/Town.h"
#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::services {

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.services.TownService");

TownService& TownService::getInstance() {
	static TownService instance; // Java SingletonHolder
	return instance;
}

TownService::TownService() {
	AION_UNPORTED();
}

runtime::Ptr<model::town::Town> TownService::getTownById(int32_t townId) {
	AION_UNPORTED();
}

int32_t TownService::getTownResidence(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

int32_t TownService::getTownIdByPosition(model::gameobjects::Creature& creature) {
	AION_UNPORTED();
}

void TownService::onEnterWorld(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::services
