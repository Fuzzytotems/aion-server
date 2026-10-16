#pragma once

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/model/broker/filter/fwd.h"
#include "aion/gameserver/model/templates/item/fwd.h"

namespace aion::gameserver::model::broker::filter {

/**
 * Filter of a broker item category (BrokerItemMask).
 * <p>
 * C++: RefCounted (fieldmap K3, `BrokerItemMask.filter`). The filters are created once for the constants of the BrokerItemMask companion
 * (BrokerItemMaskInfo.cpp) and never released, like the Java enum constants holding them.
 *
 * @author ATracer
 */
class BrokerFilter : public runtime::RefCounted {
	AION_MAKE_REF_FRIEND
protected:
	BrokerFilter() = default;
	~BrokerFilter() override;

public:
	/** @throws NullPointerException if the template is null (Java dereferences it) */
	virtual bool accept(const templates::item::ItemTemplate* template_) = 0;
};

} // namespace aion::gameserver::model::broker::filter
