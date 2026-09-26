#include "aion/gameserver/dataholders/ItemRestrictionCleanupData.h"

namespace aion::gameserver::dataholders {

int32_t ItemRestrictionCleanupData::size() const {
	return static_cast<int32_t>(getList().size());
}

const std::vector<model::templates::restriction::ItemCleanupTemplate>& ItemRestrictionCleanupData::getList() const {
	return bplist; // Java: bplist == null ? Collections.emptyList() : bplist (an absent list is the empty bound vector)
}

bool ItemRestrictionCleanupData::hasAccountOrLegionWhStorabilityDisabled(int32_t itemId) const {
	for (const model::templates::restriction::ItemCleanupTemplate& t : bplist) {
		if (t.getId() == itemId && (t.resultAccountWH() == 0 || t.resultLegionWH() == 0))
			return true;
	}
	return false;
}

} // namespace aion::gameserver::dataholders
