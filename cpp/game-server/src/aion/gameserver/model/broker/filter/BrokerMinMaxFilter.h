#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/broker/filter/BrokerFilter.h"
#include "aion/gameserver/model/broker/filter/fwd.h"
#include "aion/gameserver/model/templates/item/fwd.h"

namespace aion::gameserver::model::broker::filter {

/**
 * Accepts items whose template id divided by 100000 lies in [min, max].
 *
 * @author ATracer
 */
class BrokerMinMaxFilter : public BrokerFilter {
	AION_MAKE_REF_FRIEND
private:
	const int32_t min;
	const int32_t max;

protected:
	BrokerMinMaxFilter(int32_t min, int32_t max);
	~BrokerMinMaxFilter() override;

public:
	/** Java: new BrokerMinMaxFilter(min, max) */
	static runtime::Ref<BrokerMinMaxFilter> create(int32_t min, int32_t max);

	bool accept(const templates::item::ItemTemplate* template_) override;
};

} // namespace aion::gameserver::model::broker::filter
