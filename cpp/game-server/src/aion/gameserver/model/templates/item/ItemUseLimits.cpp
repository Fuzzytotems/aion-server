#include "aion/gameserver/model/templates/item/ItemUseLimits.h"

#include <exception>

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/world/zone/ZoneName.h"

namespace aion::gameserver::model::templates::item {

const ::aion::gameserver::world::zone::ZoneName* ItemUseLimits::getUseArea() const {
	if (usearea.empty()) // Java: usearea == null (the census finds no present-empty usearea)
		return nullptr;
	try {
		return ::aion::gameserver::world::zone::ZoneName::createOrGet(usearea);
	} catch (const runtime::UnportedException&) {
		throw; // not a Java exception: keep unported paths visible
	} catch (const std::exception&) { // Java: catch (Exception e); ZoneName.createOrGet throws only unchecked exceptions
		return nullptr;
	}
}

const std::vector<int32_t>& ItemUseLimits::getOwnershipWorldIds() const {
	static const std::vector<int32_t> EMPTY;
	if (!ownershipWorldIds.has_value())
		return EMPTY;
	return *ownershipWorldIds;
}

} // namespace aion::gameserver::model::templates::item
