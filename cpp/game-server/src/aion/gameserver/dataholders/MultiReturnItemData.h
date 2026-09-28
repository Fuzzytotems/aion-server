#pragma once

#include <cstdint>
#include <unordered_map>
#include <vector>

#include "aion/gameserver/dataholders/MultiReturnItemData.xml.h"

namespace aion::gameserver::dataholders {

/**
 * Java com.aionemu.gameserver.dataholders.MultiReturnItemData.
 * <p>
 * C++: the index points into the bound `multiReturnItemTemplate` storage, which stays after afterUnmarshal (static-data.md §2.6).
 *
 * @author ginho1
 */
class MultiReturnItemData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/MultiReturnItemData.xml.inc"
private:
	std::unordered_map<int32_t, const std::vector<model::templates::item::ReturnLocList>*> returnLocList;

public:
	int32_t size() const;

	/** @return the return locations of the item, nullptr (Java null) if there are none */
	const std::vector<model::templates::item::ReturnLocList>* getReturnLocListById(int32_t id) const;
};

} // namespace aion::gameserver::dataholders
