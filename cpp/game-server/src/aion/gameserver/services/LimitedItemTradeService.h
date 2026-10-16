#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/collections/HashMap.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/model/limiteditems/fwd.h"
#include "aion/gameserver/services/fwd.h"

namespace aion::gameserver::services {

/**
 * TYPE_A: BuyLimit == 0 && SellLimit != 0<br>
 * TYPE_B: BuyLimit != 0 && SellLimit == 0<br>
 * TYPE_C: BuyLimit != 0 && SellLimit != 0
 * <p>
 * C++: an Immortal singleton (hub-headers.md §11.2) with a private constructor and destructor; getInstance() is Java's SingletonHolder.
 *
 * @author xTz
 */
class LimitedItemTradeService : public runtime::Immortal {
private:
	runtime::HashMap<int32_t, runtime::Ref<model::limiteditems::LimitedTradeNpc>> limitedTradeNpcs{AION_LOCK_CLASS(LimitedItemTradeService::limitedTradeNpcs)}; // Java: = new HashMap<>()
	LimitedItemTradeService();
	~LimitedItemTradeService();
public:
	void start();
	runtime::Ptr<model::limiteditems::LimitedItem> getLimitedItem(int32_t itemId, int32_t npcId);
	bool isLimitedTradeNpc(int32_t npcId);
	runtime::Ptr<model::limiteditems::LimitedTradeNpc> getLimitedTradeNpc(int32_t npcId);
	static LimitedItemTradeService& getInstance(); // Java singleton
};

} // namespace aion::gameserver::services
