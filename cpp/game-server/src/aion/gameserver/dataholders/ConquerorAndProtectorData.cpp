#include "aion/gameserver/dataholders/ConquerorAndProtectorData.h"

namespace aion::gameserver::dataholders {

const model::templates::cp::CPRank* ConquerorAndProtectorData::getRank(model::templates::cp::CPType type, int32_t rank) const {
	for (const model::templates::cp::CPRank& template_ : ranks) {
		if (template_.getType() == type && template_.getRankNum() == rank)
			return &template_;
	}
	return nullptr;
}

int32_t ConquerorAndProtectorData::size() const {
	return static_cast<int32_t>(ranks.size());
}

} // namespace aion::gameserver::dataholders
