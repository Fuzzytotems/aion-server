#include "aion/gameserver/dataholders/ItemRestrictionCleanupData.h"

#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::dataholders {

int32_t ItemRestrictionCleanupData::size() const {
	AION_UNPORTED();
}

const std::vector<model::templates::restriction::ItemCleanupTemplate>& ItemRestrictionCleanupData::getList() const {
	AION_UNPORTED();
}

bool ItemRestrictionCleanupData::hasAccountOrLegionWhStorabilityDisabled(int32_t itemId) const {
	AION_UNPORTED();
}

} // namespace aion::gameserver::dataholders
