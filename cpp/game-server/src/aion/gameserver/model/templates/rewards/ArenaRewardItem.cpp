#include "aion/gameserver/model/templates/rewards/ArenaRewardItem.h"

namespace aion::gameserver::model::templates::rewards {

ArenaRewardItem::ArenaRewardItem(int32_t itemId, int32_t baseCount, int32_t rankingCount, int32_t scoreCount)
    : itemId_(itemId), baseCount_(baseCount), rankingCount_(rankingCount), scoreCount_(scoreCount) {}

ArenaRewardItem::~ArenaRewardItem() = default;

runtime::Ref<ArenaRewardItem> ArenaRewardItem::create(int32_t itemId, int32_t baseCount, int32_t rankingCount, int32_t scoreCount) {
	return runtime::makeRef<ArenaRewardItem>(itemId, baseCount, rankingCount, scoreCount);
}

int32_t ArenaRewardItem::getTotalCount() const {
	return static_cast<int32_t>(static_cast<uint32_t>(baseCount_) + static_cast<uint32_t>(rankingCount_) + static_cast<uint32_t>(scoreCount_));
}

bool ArenaRewardItem::equals(const ArenaRewardItem& obj) const {
	return itemId_ == obj.itemId_ && baseCount_ == obj.baseCount_ && rankingCount_ == obj.rankingCount_ && scoreCount_ == obj.scoreCount_;
}

int32_t ArenaRewardItem::hashCode() const {
	// java.lang.runtime.ObjectMethods: 31 * h + Integer.hashCode(component) over the components in declaration order
	uint32_t hash = 0;
	for (int32_t component : {itemId_, baseCount_, rankingCount_, scoreCount_})
		hash = 31u * hash + static_cast<uint32_t>(component);
	return static_cast<int32_t>(hash);
}

std::string ArenaRewardItem::toString() {
	return "ArenaRewardItem[itemId=" + std::to_string(itemId_) + ", baseCount=" + std::to_string(baseCount_) +
	       ", rankingCount=" + std::to_string(rankingCount_) + ", scoreCount=" + std::to_string(scoreCount_) + "]";
}

} // namespace aion::gameserver::model::templates::rewards
