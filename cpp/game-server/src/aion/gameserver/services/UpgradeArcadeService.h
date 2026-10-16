#pragma once

#include <cstdint>
#include <vector>

#include "aion/gameserver/runtime/collections/ConcurrentHashMap.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/model/event/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/templates/event/upgradearcade/fwd.h"
#include "aion/gameserver/services/fwd.h"

namespace aion::gameserver::services {

/**
 * @author ginho1, Estrayl, Neon
 */
class UpgradeArcadeService : public runtime::Immortal {
private:
	static constexpr int32_t FRENZY_POINTS_PER_TOKEN = 8;
	runtime::ConcurrentHashMap<int32_t, runtime::Ref<model::event::ArcadeProgress>> cachedProgress{
		AION_LOCK_CLASS(UpgradeArcadeService::cachedProgress#stripe)};
	runtime::Ptr<model::event::ArcadeProgress> getProgress(int32_t objId);
public:
	void start(model::gameobjects::player::Player& player, int32_t sessionId);
private:
	void sendRemainingFrenzyModeTime(model::gameobjects::player::Player& player, model::event::ArcadeProgress& progress);
public:
	void open(model::gameobjects::player::Player& player);
	void showRewardList(model::gameobjects::player::Player& player);
	std::vector<const model::templates::event::upgradearcade::ArcadeRewards*> getRewards();
	const model::templates::event::upgradearcade::ArcadeRewards* getRewardsForLevel(int32_t level);
	void startTry(model::gameobjects::player::Player& player);
private:
	void increaseFrenzyPoints(model::gameobjects::player::Player& player, model::event::ArcadeProgress& progress, int32_t frenzyPoints);
	float getUpgradeChance(int32_t currentLevel);
public:
	void resume(model::gameobjects::player::Player& player);
	void getReward(model::gameobjects::player::Player& player);
	static UpgradeArcadeService& getInstance(); // Java singleton
};

} // namespace aion::gameserver::services
