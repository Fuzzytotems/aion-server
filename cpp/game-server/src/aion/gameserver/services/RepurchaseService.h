#pragma once

#include <cstdint>
#include <unordered_set>
#include <vector>

#include "aion/gameserver/runtime/collections/ConcurrentHashMap.h"
#include "aion/gameserver/runtime/collections/Rc.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/trade/fwd.h"
#include "aion/gameserver/services/fwd.h"

namespace aion::gameserver::services {

/**
 * @author xTz
 */
class RepurchaseService : public runtime::Immortal {
private:
	runtime::ConcurrentHashMap<int32_t, runtime::Ref<runtime::RcHashSet<runtime::Ref<model::gameobjects::Item>>>> repurchaseItems{
		AION_LOCK_CLASS(RepurchaseService::repurchaseItems#stripe)};
	RepurchaseService();
public:
	/** Save items for repurchase for this player */
	void addRepurchaseItems(model::gameobjects::player::Player& player, const std::vector<runtime::Ptr<model::gameobjects::Item>>& items);
	/** Delete all repurchase items for this player */
	void removeRepurchaseItems(model::gameobjects::player::Player& player);
	std::unordered_set<runtime::Ptr<model::gameobjects::Item>> getRepurchaseItems(int32_t playerObjectId);
	bool canRepurchase(model::gameobjects::player::Player& player, int32_t itemObjectId);
	void repurchaseFromShop(model::gameobjects::player::Player& player, model::trade::RepurchaseList& repurchaseList);
	static RepurchaseService& getInstance(); // Java singleton
};

} // namespace aion::gameserver::services
