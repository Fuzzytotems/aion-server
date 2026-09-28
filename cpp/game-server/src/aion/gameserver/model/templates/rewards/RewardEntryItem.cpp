#include "aion/gameserver/model/templates/rewards/RewardEntryItem.h"

namespace aion::gameserver::model::templates::rewards {

RewardEntryItem::RewardEntryItem(int32_t entryIdValue, int32_t itemId, int64_t countValue) : RewardItem(itemId, countValue), entryId(entryIdValue) {}

RewardEntryItem::~RewardEntryItem() = default;

runtime::Ref<RewardEntryItem> RewardEntryItem::create(int32_t entryIdValue, int32_t itemId, int64_t countValue) {
	return runtime::makeRef<RewardEntryItem>(entryIdValue, itemId, countValue);
}

} // namespace aion::gameserver::model::templates::rewards
