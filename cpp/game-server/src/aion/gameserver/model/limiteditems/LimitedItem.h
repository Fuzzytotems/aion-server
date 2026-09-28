#pragma once

#include <cstdint>
#include <string>
#include <string_view>

#include "aion/gameserver/runtime/collections/HashMap.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/model/limiteditems/fwd.h"

namespace aion::gameserver::model::limiteditems {

/**
 * S0c declaration header (docs/design/hub-headers.md §3.5), written for the member-type guards waiting for it at the spine freeze.
 *
 * @author xTz, Neon
 */
class LimitedItem : public runtime::RefCounted {
	AION_MAKE_REF_FRIEND
private:
	runtime::Field<int32_t> itemId{};
	runtime::Field<int32_t> sellLimit{};
	const int32_t buyLimit;
	const int32_t defaultSellLimit;
	const std::string salesTime;
	runtime::HashMap<int32_t, int32_t> buyCounts{AION_LOCK_CLASS(LimitedItem::buyCounts)}; // Java: = new HashMap<>()

protected:
	LimitedItem(int32_t itemId, int32_t sellLimit, int32_t buyLimit, std::string_view salesTime);

public:
	static runtime::Ref<LimitedItem> create(int32_t value, int32_t sellLimitValue, int32_t buyLimitValue, std::string_view salesTimeValue);

	int32_t getItemId() const { return this->itemId.get(); }

	void setBuyCount(int32_t playerObjectId, int32_t count);

	int32_t getBuyCount(int32_t playerObjectId);

	void setItem(int32_t value) { this->itemId.set(value); }

	int32_t getSellLimit() const { return this->sellLimit.get(); }

	int32_t getBuyLimit() const { return this->buyLimit; }

	void setToDefault();

	void setSellLimit(int32_t value) { this->sellLimit.set(value); }

	int32_t getDefaultSellLimit() const { return this->defaultSellLimit; }

	std::string getSalesTime() const { return this->salesTime; }

protected:
	~LimitedItem() override;
};

} // namespace aion::gameserver::model::limiteditems
