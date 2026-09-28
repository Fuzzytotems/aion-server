#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/craft/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/services/craft/fwd.h"

namespace aion::gameserver::services::craft {

/**
 * @author synchro2
 */
class RelinquishCraftStatus {
private:
	static constexpr int32_t expertMinValue = 400;
	static constexpr int32_t expertMaxValue = 499;
	static constexpr int32_t masterMinValue = 500;
	static constexpr int32_t masterMaxValue = 549;
	static constexpr int32_t expertPrice = 120895;
	static constexpr int32_t masterPrice = 3497448;
	static constexpr int32_t skillMessageId = 1401127;
public:
	static bool relinquishExpertStatus(model::gameobjects::player::Player& player, model::craft::Profession profession);
	static bool relinquishExpertStatus(model::gameobjects::player::Player& player, model::craft::Profession profession, int32_t price);
	static bool relinquishMasterStatus(model::gameobjects::player::Player& player, model::craft::Profession profession);
	static bool relinquishMasterStatus(model::gameobjects::player::Player& player, model::craft::Profession profession, int32_t price);
private:
	static bool relinquishCraftStatus(model::gameobjects::player::Player& player, model::craft::Profession profession, int32_t minSkillLevel,
		int32_t maxSkillLevel, int32_t price);
	static bool decreaseKinah(model::gameobjects::player::Player& player, int32_t basePrice);
public:
	static void removeRecipesAbove(model::gameobjects::player::Player& player, int32_t skillId, int32_t level);
	static void deleteCraftStatusQuests(int32_t skillId, model::gameobjects::player::Player& player, bool isExpert);
	static void removeExcessCraftStatus(model::gameobjects::player::Player& player, bool isExpert);
};

} // namespace aion::gameserver::services::craft
