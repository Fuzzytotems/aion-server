#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/collections/ArrayList.h"
#include "aion/gameserver/runtime/collections/Rc.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/templates/rewards/fwd.h"
#include "aion/gameserver/services/reward/fwd.h"

namespace aion::gameserver::services::reward {

/**
 * @author Neon
 */
class VeteranRewardService final : public runtime::Immortal {
private:
	/** C++: rewards and randomRewards are defined in VeteranRewardService.cpp (the element destructors need the complete RewardItem) */
	static runtime::ArrayList<runtime::Ref<runtime::RcArrayList<runtime::Ref<model::templates::rewards::RewardItem>>>> rewards;
	static runtime::ArrayList<runtime::Ref<model::templates::rewards::RewardItem>> randomRewards; // Java: = new ArrayList<>()
	static constexpr int32_t RANDOM_ITEMS_PER_MONTH = 4;
	// Java static initializer block (VeteranRewardService.java:27): fills rewards and randomRewards with RewardItem literals; the port fills them
	// where the .cpp defines the two lists (plain data, no static data or services needed).
	/** Prevent instantiation */
	VeteranRewardService();
public:
	static VeteranRewardService& getInstance(); // Java singleton
	void tryReward(model::gameobjects::player::Player& player);
};

} // namespace aion::gameserver::services::reward
