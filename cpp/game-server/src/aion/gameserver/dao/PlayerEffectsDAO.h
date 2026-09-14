#pragma once

#include <cstdint>
#include <string_view>
#include <unordered_set>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/sched/PinnedCallback.h"
#include "aion/gameserver/dao/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/skillengine/model/fwd.h"

namespace aion::gameserver::dao {

/**
 * @author ATracer
 */
class PlayerEffectsDAO {
private:
	/** Java: effect -> effect.canSaveOnLogout() && effect.getRemainingTimeMillis() > 28000 (defined in PlayerEffectsDAO.cpp) */
	static const runtime::PinnedCallback<bool(skillengine::model::Effect&)> insertableEffectsPredicate;
public:
	static void loadPlayerEffects(model::gameobjects::player::Player& player);
	static void storePlayerEffects(model::gameobjects::player::Player& player);
private:
	/** Magical criticals are stored as one bit per effect position, bit 0 being position 1. */
	static int32_t encodeMagicalCriticalPositions(skillengine::model::Effect& effect);
	static std::unordered_set<int32_t> decodeMagicalCriticalPositions(int32_t bits);
	static void deletePlayerEffects(model::gameobjects::player::Player& player);
};

} // namespace aion::gameserver::dao
