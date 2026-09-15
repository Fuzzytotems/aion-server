#pragma once

#include <cstdint>
#include <unordered_map>

#include "aion/gameserver/dataholders/ItemSetData.xml.h"

namespace aion::gameserver::dataholders {

/**
 * Java com.aionemu.gameserver.dataholders.ItemSetData.
 * <p>
 * C++: the @XmlTransient index maps point into the bound `itemsetList` storage, which stays after afterUnmarshal (static-data.md §2.6; Java
 * sets the list to null). Declarations beyond Java's getItemSetTemplateByItemId come with the P4-09 port (header request pre-2).
 *
 * @author ATracer
 */
class ItemSetData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/ItemSetData.xml.inc"
private:
	std::unordered_map<int32_t, const model::templates::itemset::ItemSetTemplate*> sets;
	std::unordered_map<int32_t, const model::templates::itemset::ItemSetTemplate*> setItems;

public:
	/** @return the item set the item belongs to, nullptr (Java null) if there is none */
	const model::templates::itemset::ItemSetTemplate* getItemSetTemplateByItemId(int32_t itemId) const;
};

} // namespace aion::gameserver::dataholders
