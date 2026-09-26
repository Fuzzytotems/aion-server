#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/collections/ArrayList.h"
#include "aion/gameserver/runtime/collections/HashMap.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/trade/fwd.h"

namespace aion::gameserver::model::trade {

/**
 * The items a player buys from or sells to an npc, a private store or a pet, with the price computed for them.
 * <p>
 * C++: RefCounted (fieldmap K4, packet member `CM_BUY_ITEM.tradeList`), created with create(). getTradeItems and getRequiredItems return the
 * live collections like Java.
 *
 * @author ATracer, Wakizashi, Neon
 */
class TradeList : public runtime::RefCounted {
	AION_MAKE_REF_FRIEND
private:
	const int32_t sellerObjId;
	runtime::ArrayList<runtime::Ref<TradeItem>> tradeItems{AION_LOCK_CLASS(TradeList::tradeItems)};
	runtime::Field<int64_t> requiredKinah{};
	runtime::Field<int32_t> requiredAp{};
	runtime::LinkedHashMap<int32_t, int64_t> requiredItems{AION_LOCK_CLASS(TradeList::requiredItems)};

protected:
	TradeList();
	explicit TradeList(int32_t sellerObjId);
	~TradeList() override;

public:
	/** Java: new TradeList() */
	static runtime::Ref<TradeList> create();

	/** Java: new TradeList(sellerObjId) */
	static runtime::Ref<TradeList> create(int32_t sellerObjId);

	void addItem(int32_t itemId, int64_t count);

	void addTradeItem(TradeItem& tradeItem);

	/** @return price TradeList sum price */
	bool calculateBuyListPrice(gameobjects::player::Player& player, int32_t modifier);

	bool calculateAbyssRewardBuyList(gameobjects::player::Player& player, int32_t modifier);

	runtime::ArrayList<runtime::Ref<TradeItem>>& getTradeItems() { return tradeItems; }

	int32_t size();

	int32_t getSellerObjId() const { return sellerObjId; }

	int32_t getRequiredAp() const { return requiredAp.get(); }

	int64_t getRequiredKinah() const { return requiredKinah.get(); }

	runtime::LinkedHashMap<int32_t, int64_t>& getRequiredItems() { return requiredItems; }
};

} // namespace aion::gameserver::model::trade
