#include "aion/gameserver/services/craft/RelinquishCraftStatus.h"

#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::services::craft {

bool RelinquishCraftStatus::relinquishExpertStatus(model::gameobjects::player::Player& player, model::craft::Profession profession) {
	AION_UNPORTED();
}

bool RelinquishCraftStatus::relinquishExpertStatus(model::gameobjects::player::Player& player, model::craft::Profession profession, int32_t price) {
	AION_UNPORTED();
}

bool RelinquishCraftStatus::relinquishMasterStatus(model::gameobjects::player::Player& player, model::craft::Profession profession) {
	AION_UNPORTED();
}

bool RelinquishCraftStatus::relinquishMasterStatus(model::gameobjects::player::Player& player, model::craft::Profession profession, int32_t price) {
	AION_UNPORTED();
}

bool RelinquishCraftStatus::relinquishCraftStatus(model::gameobjects::player::Player& player, model::craft::Profession profession,
	int32_t minSkillLevel, int32_t maxSkillLevel, int32_t price) {
	AION_UNPORTED();
}

bool RelinquishCraftStatus::decreaseKinah(model::gameobjects::player::Player& player, int32_t basePrice) {
	AION_UNPORTED();
}

void RelinquishCraftStatus::removeRecipesAbove(model::gameobjects::player::Player& player, int32_t skillId, int32_t level) {
	AION_UNPORTED();
}

void RelinquishCraftStatus::deleteCraftStatusQuests(int32_t skillId, model::gameobjects::player::Player& player, bool isExpert) {
	AION_UNPORTED();
}

void RelinquishCraftStatus::removeExcessCraftStatus(model::gameobjects::player::Player& player, bool isExpert) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::services::craft
