#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/model/drop/fwd.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/services/drop/fwd.h"

namespace aion::gameserver::services::drop {

/**
 * C++: an Immortal singleton (hub-headers.md §11.2) with a private constructor and destructor; getInstance() is Java's SingletonHolder.
 *
 * @author xTz, Sykra
 */
class DropDistributionService : public runtime::Immortal {
private:
	DropDistributionService();
	~DropDistributionService();
public:
	static DropDistributionService& getInstance(); // Java singleton
	void handleRollOrBid(runtime::Ptr<model::gameobjects::player::Player> player, int32_t mode, int32_t roll, int64_t bid, int32_t itemId, int32_t npcObjId, int32_t index);
private:
	void handleRoll(model::gameobjects::player::Player& player, int32_t roll, int32_t itemId, model::drop::DropItem& requestedItem, model::gameobjects::DropNpc& dropNpc);
	void handleBid(model::gameobjects::player::Player& player, int64_t bid, int32_t itemId, model::drop::DropItem& requestedItem, model::gameobjects::DropNpc& dropNpc);
	void distributeLoot(model::gameobjects::player::Player& player, int64_t luckyPlayer, int32_t itemId, model::drop::DropItem& requestedItem, model::gameobjects::DropNpc& dropNpc);
};

} // namespace aion::gameserver::services::drop
