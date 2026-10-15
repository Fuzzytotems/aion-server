#pragma once

#include <cstdint>
#include <unordered_map>

#include "aion/gameserver/dataholders/WarehouseExpandData.xml.h"

namespace aion::gameserver::dataholders {

/**
 * Java com.aionemu.gameserver.dataholders.WarehouseExpandData.
 * <p>
 * C++: the index points into the bound `expansionTemplates` storage, which stays after afterUnmarshal (static-data.md §2.6).
 *
 * @author spufy
 */
class WarehouseExpandData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/WarehouseExpandData.xml.inc"
private:
	std::unordered_map<int32_t, const model::templates::StorageExpansionTemplate*> expansionTemplatesByNpcId;

public:
	int32_t size() const;

	/** @return the expansion template of the npc, nullptr (Java null) if there is none */
	const model::templates::StorageExpansionTemplate* getWarehouseExpansionTemplate(int32_t id) const;
};

} // namespace aion::gameserver::dataholders
