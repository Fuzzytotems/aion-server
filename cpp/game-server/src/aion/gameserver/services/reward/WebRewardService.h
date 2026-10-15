#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/collections/CopyOnWriteArrayList.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/templates/rewards/fwd.h"
#include "aion/gameserver/services/reward/fwd.h"

namespace aion::gameserver::services::reward {

/**
 * @author KID, Neon
 */
class WebRewardService : public runtime::Immortal {
public:
	class MaxLevelReward {
	public:
		static inline runtime::CopyOnWriteArraySet<int32_t> pendingAscension{AION_LOCK_CLASS(WebRewardService::MaxLevelReward::pendingAscension)};
		static bool isPendingAscension(model::gameobjects::player::Player& player);
		static bool reward(model::gameobjects::player::Player& player);
		static void addBasicGear(model::gameobjects::player::Player& player);
	};
private:
public:
	static WebRewardService& getInstance(); // Java singleton
private:
	WebRewardService();
public:
	void sendAvailableRewards(runtime::Ptr<model::gameobjects::player::Player> player);
private:
	bool sendRewardItem(model::gameobjects::player::Player& player, model::templates::rewards::RewardEntryItem& item);
	bool executeRewardAction(model::gameobjects::player::Player& player, model::templates::rewards::RewardEntryItem& rewardItem);
};

} // namespace aion::gameserver::services::reward
