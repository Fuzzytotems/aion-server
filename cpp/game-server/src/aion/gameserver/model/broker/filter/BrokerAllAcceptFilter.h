#pragma once

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/broker/filter/BrokerFilter.h"
#include "aion/gameserver/model/broker/filter/fwd.h"
#include "aion/gameserver/model/templates/item/fwd.h"

namespace aion::gameserver::model::broker::filter {

/**
 * Accepts every item.
 *
 * @author ATracer
 */
class BrokerAllAcceptFilter : public BrokerFilter {
	AION_MAKE_REF_FRIEND
protected:
	/** Java: implicit default constructor */
	BrokerAllAcceptFilter() = default;
	~BrokerAllAcceptFilter() override;

public:
	/** Java: new BrokerAllAcceptFilter() */
	static runtime::Ref<BrokerAllAcceptFilter> create();

	bool accept(const templates::item::ItemTemplate* template_) override;
};

} // namespace aion::gameserver::model::broker::filter
