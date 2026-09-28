#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/collections/HashMap.h"
#include "aion/gameserver/runtime/collections/Rc.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/templates/rewards/fwd.h"
#include "aion/gameserver/services/reward/fwd.h"

namespace aion::gameserver::services::reward {

/**
 * Created on 29.05.2016
 *
 * @author Estrayl
 * @since AION 4.8
 */
class StarterKitService : public runtime::Immortal {
private:
	runtime::LinkedHashMap<int32_t, runtime::Ref<runtime::RcArrayList<runtime::Ref<model::templates::rewards::RewardItem>>>> itemMap{
		AION_LOCK_CLASS(StarterKitService::itemMap)};
public:
	static StarterKitService& getInstance(); // Java singleton
private:
	StarterKitService();
public:
	void onLevelUp(model::gameobjects::player::Player& player, int32_t fromLevel, int32_t toLevel);
};

} // namespace aion::gameserver::services::reward
