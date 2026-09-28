#include "aion/gameserver/services/StaticDoorService.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/commons/logging/LoggerFactory.h"

namespace aion::gameserver::services {

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.services.StaticDoorService");

StaticDoorService& StaticDoorService::getInstance() {
	static StaticDoorService instance; // Java SingletonHolder
	return instance;
}

void StaticDoorService::openStaticDoor(model::gameobjects::player::Player& player, int32_t doorId) {
	AION_UNPORTED();
}

void StaticDoorService::changeStaticDoorState(model::gameobjects::player::Player& player, int32_t doorId, bool open, int32_t state) {
	AION_UNPORTED();
}

runtime::Ptr<model::gameobjects::StaticDoor> StaticDoorService::getDoor(model::gameobjects::player::Player& player, int32_t doorId) {
	AION_UNPORTED();
}

bool StaticDoorService::checkStaticDoorKey(model::gameobjects::player::Player& player, model::gameobjects::StaticDoor& door, int32_t keyId) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::services
