#include "aion/gameserver/model/autogroup/AutoInstance.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/model/autogroup/AGPlayer.h"
#include "aion/gameserver/world/WorldMapInstance.h"

namespace aion::gameserver::model::autogroup {

AutoInstance::AutoInstance(AutoGroupType value)
	: agt(value) {
}

bool AutoInstance::removeItem(gameobjects::player::Player& player, int32_t itemId, int64_t requiredCount) {
	AION_UNPORTED();
}

void AutoInstance::onInstanceCreate(world::WorldMapInstance& value) {
	AION_UNPORTED();
}

AGQuestion AutoInstance::addLookingForParty(LookingForParty& lookingForParty) {
	AION_UNPORTED();
}

void AutoInstance::onEnterInstance(gameobjects::player::Player& player) {
	AION_UNPORTED();
}

void AutoInstance::onLeaveInstance(gameobjects::player::Player& player) {
	AION_UNPORTED();
}

void AutoInstance::onPressEnter(gameobjects::player::Player& player) {
	AION_UNPORTED();
}

void AutoInstance::unregister(gameobjects::player::Player& player) {
	AION_UNPORTED();
}

bool AutoInstance::isRegistrationDisabled(LookingForParty& lfp) {
	AION_UNPORTED();
}

std::vector<runtime::Ptr<AGPlayer>> AutoInstance::getAGPlayersByRace(Race race) {
	AION_UNPORTED();
}

std::vector<runtime::Ptr<AGPlayer>> AutoInstance::getAGPlayersByClass(PlayerClass playerClass) {
	AION_UNPORTED();
}

std::vector<runtime::Ptr<gameobjects::player::Player>> AutoInstance::getPlayersByRace(Race race) {
	AION_UNPORTED();
}

int32_t AutoInstance::getMaxPlayers() {
	AION_UNPORTED();
}

AutoInstance::~AutoInstance() = default;

} // namespace aion::gameserver::model::autogroup
