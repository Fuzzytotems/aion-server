#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/services/player/fwd.h"

namespace aion::gameserver::services::player {

/**
 * @author Jego, xTz
 */
class PlayerReviveService {
public:
	static void duelRevive(model::gameobjects::player::Player& player);
	static void skillRevive(model::gameobjects::player::Player& player);
	static void rebirthRevive(model::gameobjects::player::Player& player);
	static void bindRevive(model::gameobjects::player::Player& player);
	static void bindRevive(model::gameobjects::player::Player& player, int32_t skillId);
	static void kiskRevive(model::gameobjects::player::Player& player);
	static void kiskRevive(model::gameobjects::player::Player& player, int32_t skillId);
	static void instanceRevive(model::gameobjects::player::Player& player);
	static void instanceRevive(model::gameobjects::player::Player& player, int32_t skillId);
	static void revive(model::gameobjects::player::Player& player, int32_t hpPercent, int32_t mpPercent, bool setSoulSickness,
		int32_t resurrectionSkill);
	static void itemSelfRevive(model::gameobjects::player::Player& player);
	static void scheduleReviveAtBase(model::gameobjects::player::Player& player, int32_t delayMillis, int32_t skillId);
};

} // namespace aion::gameserver::services::player
