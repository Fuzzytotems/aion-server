#pragma once

#include <cstdint>
#include <initializer_list>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/broker/filter/BrokerContainsFilter.h"
#include "aion/gameserver/model/broker/filter/fwd.h"
#include "aion/gameserver/model/templates/item/fwd.h"

namespace aion::gameserver::model::broker::filter {

/**
 * Accepts craft designs (BrokerContainsFilter) whose recipe belongs to a craft skill.
 *
 * @author xTz
 */
class BrokerRecipeFilter : public BrokerContainsFilter {
	AION_MAKE_REF_FRIEND
private:
	const int32_t craftSkillId;

protected:
	/** Java: public BrokerRecipeFilter(int craftSkillId, int... masks) */
	BrokerRecipeFilter(int32_t craftSkillId, std::initializer_list<int32_t> masks);
	~BrokerRecipeFilter() override;

public:
	/** Java: new BrokerRecipeFilter(craftSkillId, masks...) */
	static runtime::Ref<BrokerRecipeFilter> create(int32_t craftSkillId, std::initializer_list<int32_t> masks);

	bool accept(const templates::item::ItemTemplate* template_) override;
};

} // namespace aion::gameserver::model::broker::filter
