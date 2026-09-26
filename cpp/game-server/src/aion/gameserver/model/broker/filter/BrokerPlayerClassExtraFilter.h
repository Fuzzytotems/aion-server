#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/PlayerClass.h"
#include "aion/gameserver/model/broker/filter/BrokerPlayerClassFilter.h"
#include "aion/gameserver/model/broker/filter/fwd.h"
#include "aion/gameserver/model/templates/item/fwd.h"

namespace aion::gameserver::model::broker::filter {

/**
 * Accepts items specific to a player class whose template id divided by 100000 equals the mask.
 *
 * @author ATracer
 */
class BrokerPlayerClassExtraFilter : public BrokerPlayerClassFilter {
	AION_MAKE_REF_FRIEND
private:
	const int32_t mask;

protected:
	BrokerPlayerClassExtraFilter(int32_t mask, PlayerClass playerClass);
	~BrokerPlayerClassExtraFilter() override;

public:
	/** Java: new BrokerPlayerClassExtraFilter(mask, playerClass) */
	static runtime::Ref<BrokerPlayerClassExtraFilter> create(int32_t mask, PlayerClass playerClass);

	bool accept(const templates::item::ItemTemplate* template_) override;
};

} // namespace aion::gameserver::model::broker::filter
