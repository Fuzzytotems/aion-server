#include "aion/gameserver/model/items/ItemCooldown.h"

namespace aion::gameserver::model::items {

ItemCooldown::ItemCooldown(int64_t timeValue, int32_t useDelayValue) : time(timeValue), useDelay(useDelayValue) {
}

ItemCooldown::~ItemCooldown() = default;

runtime::Ref<ItemCooldown> ItemCooldown::create(int64_t timeValue, int32_t useDelayValue) {
	return runtime::makeRef<ItemCooldown>(timeValue, useDelayValue);
}

} // namespace aion::gameserver::model::items
