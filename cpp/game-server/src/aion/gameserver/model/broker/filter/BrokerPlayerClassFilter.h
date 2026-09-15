#pragma once

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/PlayerClass.h"
#include "aion/gameserver/model/broker/filter/BrokerFilter.h"
#include "aion/gameserver/model/broker/filter/fwd.h"
#include "aion/gameserver/model/templates/item/fwd.h"

namespace aion::gameserver::model::broker::filter {

/**
 * Accepts items specific to a player class.
 *
 * @author ATracer
 */
class BrokerPlayerClassFilter : public BrokerFilter {
	AION_MAKE_REF_FRIEND
private:
	const PlayerClass playerClass;

protected:
	explicit BrokerPlayerClassFilter(PlayerClass playerClass);
	~BrokerPlayerClassFilter() override;

public:
	/** Java: new BrokerPlayerClassFilter(playerClass) */
	static runtime::Ref<BrokerPlayerClassFilter> create(PlayerClass playerClass);

	bool accept(const templates::item::ItemTemplate* template_) override;
};

} // namespace aion::gameserver::model::broker::filter
