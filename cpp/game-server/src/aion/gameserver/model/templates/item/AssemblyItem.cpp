#include "aion/gameserver/model/templates/item/AssemblyItem.h"

namespace aion::gameserver::model::templates::item {

const std::vector<int32_t>& AssemblyItem::getParts() const {
	static const std::vector<int32_t> EMPTY; // Java: `parts = new ArrayList<>()` on a template, never observable as a different list
	return parts.has_value() ? *parts : EMPTY;
}

} // namespace aion::gameserver::model::templates::item
