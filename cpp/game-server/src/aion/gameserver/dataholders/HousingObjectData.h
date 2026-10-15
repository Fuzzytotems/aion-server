#pragma once

#include <cstdint>
#include <unordered_map>

#include "aion/gameserver/dataholders/HousingObjectData.xml.h"

namespace aion::gameserver::dataholders {

/**
 * Java com.aionemu.gameserver.dataholders.HousingObjectData.
 * <p>
 * C++: the index points into the bound `housingObjects` storage, which stays after afterUnmarshal (static-data.md §2.6).
 *
 * @author Rolandas
 */
class HousingObjectData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/HousingObjectData.xml.inc"
private:
	std::unordered_map<int32_t, const model::templates::housing::PlaceableHouseObject*> objectTemplatesById;

public:
	int32_t size() const;

	/** @return the object template, nullptr (Java null) if there is none */
	const model::templates::housing::PlaceableHouseObject* getTemplateById(int32_t templateId) const;
};

} // namespace aion::gameserver::dataholders
