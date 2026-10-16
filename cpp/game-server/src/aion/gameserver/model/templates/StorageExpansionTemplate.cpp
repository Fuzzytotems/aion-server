#include "aion/gameserver/model/templates/StorageExpansionTemplate.h"

#include <algorithm>

namespace aion::gameserver::model::templates {

int32_t StorageExpansionTemplate::getMinExpansionLevel() const {
	if (expansions.empty())
		return 0;
	return std::ranges::min_element(expansions, {}, &expand::Expand::getLevel)->getLevel();
}

int32_t StorageExpansionTemplate::getMaxExpansionLevel() const {
	if (expansions.empty())
		return 0;
	return std::ranges::max_element(expansions, {}, &expand::Expand::getLevel)->getLevel();
}

std::optional<int32_t> StorageExpansionTemplate::getPrice(int32_t level) const {
	for (const expand::Expand& expand : expansions) {
		if (expand.getLevel() == level)
			return expand.getPrice();
	}
	return std::nullopt;
}

} // namespace aion::gameserver::model::templates
