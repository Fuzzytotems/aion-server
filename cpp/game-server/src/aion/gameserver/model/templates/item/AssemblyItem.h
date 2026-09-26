#pragma once

#include <cstdint>
#include <vector>

#include "aion/gameserver/model/templates/item/AssemblyItem.xml.h"

namespace aion::gameserver::model::templates::item {

/** Java com.aionemu.gameserver.model.templates.item.AssemblyItem. @author xTz */
class AssemblyItem : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/item/AssemblyItem.xml.inc"
public:
	/** @return the parts; Java creates an empty list on first use when the attribute is absent (the attribute is required) */
	const std::vector<int32_t>& getParts() const;
};

} // namespace aion::gameserver::model::templates::item
