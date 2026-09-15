#include "aion/gameserver/model/templates/rewards/RewardItem.h"

#include <string>

namespace aion::gameserver::model::templates::rewards {

RewardItem::RewardItem(int32_t value, int64_t countValue) : id(value), count(countValue) {}

runtime::Ref<RewardItem> RewardItem::create(int32_t value, int64_t countValue) {
	return runtime::makeRef<RewardItem>(value, countValue);
}

std::string RewardItem::toString() {
	return "RewardItem [id=" + std::to_string(id) + ", count=" + std::to_string(count) + "]";
}

RewardItem::~RewardItem() = default;

} // namespace aion::gameserver::model::templates::rewards
