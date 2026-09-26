#pragma once

#include <cstdint>
#include <vector>

#include "aion/gameserver/dataholders/ItemRestrictionCleanupData.xml.h"

namespace aion::gameserver::dataholders {

/** Java com.aionemu.gameserver.dataholders.ItemRestrictionCleanupData. @author KID */
class ItemRestrictionCleanupData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/ItemRestrictionCleanupData.xml.inc"
public:
	int32_t size() const;

	/** Java: an empty list for a null bplist (the bound vector is never null) */
	const std::vector<model::templates::restriction::ItemCleanupTemplate>& getList() const;

	bool hasAccountOrLegionWhStorabilityDisabled(int32_t itemId) const;
};

} // namespace aion::gameserver::dataholders
