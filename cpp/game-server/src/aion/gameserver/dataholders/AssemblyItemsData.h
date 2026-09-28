#pragma once

#include <cstdint>

#include "aion/gameserver/dataholders/AssemblyItemsData.xml.h"

namespace aion::gameserver::dataholders {

/** Java com.aionemu.gameserver.dataholders.AssemblyItemsData. @author xTz */
class AssemblyItemsData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/AssemblyItemsData.xml.inc"
public:
	int32_t size() const;

	/** @return the first assembly item with the id, nullptr (Java null) if there is none */
	const model::templates::item::AssemblyItem* getAssemblyItem(int32_t itemId) const;
};

} // namespace aion::gameserver::dataholders
