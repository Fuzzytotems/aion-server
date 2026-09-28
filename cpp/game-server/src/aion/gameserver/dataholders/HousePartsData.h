#pragma once

#include <cstdint>
#include <unordered_map>

#include "aion/gameserver/dataholders/HousePartsData.xml.h"

namespace aion::gameserver::dataholders {

/**
 * Java com.aionemu.gameserver.dataholders.HousePartsData.
 * <p>
 * C++: the index points into the bound `houseParts` storage, which stays after afterUnmarshal (static-data.md §2.6).
 *
 * @author Rolandas
 */
class HousePartsData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/HousePartsData.xml.inc"
private:
	std::unordered_map<int32_t, const model::templates::housing::HousePart*> partsById;

public:
	/** @return the house part, nullptr (Java null) if there is none */
	const model::templates::housing::HousePart* getPartById(int32_t partId) const;

	int32_t size() const;
};

} // namespace aion::gameserver::dataholders
