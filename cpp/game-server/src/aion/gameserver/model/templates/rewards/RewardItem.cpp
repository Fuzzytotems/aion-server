#include "aion/gameserver/model/templates/rewards/RewardItem.h"

#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::model::templates::rewards {

RewardItem::RewardItem(int32_t value, int64_t countValue)
	: id(value), count(countValue) {
}

runtime::Ref<RewardItem> RewardItem::create(int32_t value, int64_t countValue) {
	return runtime::makeRef<RewardItem>(value, countValue);
}

std::string RewardItem::toString() {
	AION_UNPORTED();
}

RewardItem::~RewardItem() = default;

} // namespace aion::gameserver::model::templates::rewards
