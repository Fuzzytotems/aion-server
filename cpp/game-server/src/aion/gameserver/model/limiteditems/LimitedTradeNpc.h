#pragma once

#include <vector>

#include "aion/gameserver/runtime/collections/ArrayList.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/model/limiteditems/fwd.h"

namespace aion::gameserver::model::limiteditems {

/**
 * S0c declaration header (docs/design/hub-headers.md §3.5), written for the member-type guards waiting for it at the spine freeze.
 *
 * @author xTz
 */
class LimitedTradeNpc : public runtime::RefCounted {
	AION_MAKE_REF_FRIEND
private:
	runtime::ArrayList<runtime::Ref<LimitedItem>> limitedItems{AION_LOCK_CLASS(LimitedTradeNpc::limitedItems)}; // Java: = new ArrayList<>()

protected:
	/** Java: the implicit default constructor */
	LimitedTradeNpc();

public:
	static runtime::Ref<LimitedTradeNpc> create();

	void addLimitedItems(const std::vector<runtime::Ptr<LimitedItem>>& limitedItems);

	runtime::ArrayList<runtime::Ref<LimitedItem>>& getLimitedItems() { return this->limitedItems; }

protected:
	~LimitedTradeNpc() override;
};

} // namespace aion::gameserver::model::limiteditems
