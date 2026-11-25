#pragma once

#include <cstdint>
#include <unordered_map>
#include <vector>

#include "aion/gameserver/dataholders/DecomposableItemsData.xml.h"

namespace aion::gameserver::dataholders {

/**
 * Java com.aionemu.gameserver.dataholders.DecomposableItemsData (upstream d254ea172, bad68ed81).
 * <p>
 * C++: the index points into the bound `decomposableItems` storage, which stays after afterUnmarshal (static-data.md §2.6; Java nulls the
 * list). afterUnmarshal always indexes: Java skips it when JAXBUtil.deserialize runs without a StaticDataListener (Reload.java) and indexes in
 * setData afterwards, while the C++ //reload binds the folder's files into one holder through the static data loader and republishes it, so
 * setData is not ported (docs/deviations/P4-09.md).
 *
 * @author antness
 */
class DecomposableItemsData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/DecomposableItemsData.xml.inc"
private:
	std::unordered_map<int32_t, const model::templates::item::DecomposableItem*> decomposableItemById;
	int32_t overrideCount_ = 0;

public:
	int32_t size() const;

	int32_t overrideCount() const;

	/** @return the decomposable item, nullptr (Java null) if there is none */
	const model::templates::item::DecomposableItem* get(int32_t itemId) const;
};

} // namespace aion::gameserver::dataholders
