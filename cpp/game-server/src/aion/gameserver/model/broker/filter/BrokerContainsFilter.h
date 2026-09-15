#pragma once

#include <cstdint>
#include <initializer_list>

#include "aion/gameserver/runtime/fields/Array.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/broker/filter/BrokerFilter.h"
#include "aion/gameserver/model/broker/filter/fwd.h"
#include "aion/gameserver/model/templates/item/fwd.h"

namespace aion::gameserver::model::broker::filter {

/**
 * Accepts items whose template id divided by 100000 is one of the masks.
 *
 * @author ATracer
 */
class BrokerContainsFilter : public BrokerFilter {
	AION_MAKE_REF_FRIEND
private:
	const runtime::Ref<runtime::Array<int32_t>> masks;

protected:
	/** Java: public BrokerContainsFilter(int... masks) */
	explicit BrokerContainsFilter(std::initializer_list<int32_t> masks);
	~BrokerContainsFilter() override;

public:
	/** Java: new BrokerContainsFilter(masks...) */
	static runtime::Ref<BrokerContainsFilter> create(std::initializer_list<int32_t> masks);

	bool accept(const templates::item::ItemTemplate* template_) override;
};

} // namespace aion::gameserver::model::broker::filter
