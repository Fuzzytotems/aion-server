#include "aion/gameserver/model/templates/quest/Rewards.h"

namespace aion::gameserver::model::templates::quest {

const std::vector<int32_t>& Rewards::getCollectItemChecks() const {
	static const std::vector<int32_t> empty;
	return collectItemChecks ? *collectItemChecks : empty;
}

} // namespace aion::gameserver::model::templates::quest
