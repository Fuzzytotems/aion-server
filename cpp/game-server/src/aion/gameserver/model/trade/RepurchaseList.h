#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/collections/HashSet.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/trade/fwd.h"

namespace aion::gameserver::model::trade {

/**
 * The sold items a player buys back from an npc.
 * <p>
 * C++: RefCounted (fieldmap K4, packet member `CM_BUY_ITEM.repurchaseList`), created with create(). getRepurchaseItems returns the live set
 * like Java.
 *
 * @author xTz
 */
class RepurchaseList : public runtime::RefCounted {
	AION_MAKE_REF_FRIEND
private:
	const int32_t sellerObjId;
	runtime::LinkedHashSet<int32_t> repurchases{AION_LOCK_CLASS(RepurchaseList::repurchases)};

protected:
	explicit RepurchaseList(int32_t sellerObjId);
	~RepurchaseList() override;

public:
	/** Java: new RepurchaseList(sellerObjId) */
	static runtime::Ref<RepurchaseList> create(int32_t sellerObjId);

	void addRepurchaseItem(gameobjects::player::Player& player, int32_t itemObjectId, int64_t count);

	runtime::LinkedHashSet<int32_t>& getRepurchaseItems() { return repurchases; }

	int32_t size();

	/** Java final */
	int32_t getSellerObjId() const { return sellerObjId; }
};

} // namespace aion::gameserver::model::trade
