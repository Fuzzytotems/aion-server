#pragma once

#include <cstdint>
#include <vector>

#include "aion/gameserver/model/templates/item/ItemUseLimits.xml.h"
#include "aion/gameserver/world/zone/fwd.h"

namespace aion::gameserver::model::templates::item {

/** Java com.aionemu.gameserver.model.templates.item.ItemUseLimits. @author Rolandas */
class ItemUseLimits : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/item/ItemUseLimits.xml.inc"
public:
	/** @return the interned zone name of `usearea`, nullptr (Java null) if absent or if ZoneName.createOrGet throws */
	const ::aion::gameserver::world::zone::ZoneName* getUseArea() const;

	/** @return the ownership world ids, an empty list (Java Collections.emptyList()) if absent */
	const std::vector<int32_t>& getOwnershipWorldIds() const;

	bool isRideUsable() const { return rideUsable.value_or(false); }

	bool verifyRank(int32_t rank) const { return minRank <= rank && maxRank >= rank; }
};

} // namespace aion::gameserver::model::templates::item
