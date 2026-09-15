#pragma once

#include <cstdint>
#include <unordered_map>

#include "aion/gameserver/dataholders/ItemData.xml.h"

namespace aion::gameserver::dataholders {

/**
 * Java com.aionemu.gameserver.dataholders.ItemData.
 * <p>
 * C++: the @XmlTransient index points into the bound `its` storage, which stays after afterUnmarshal (static-data.md §2.6; Java sets the list
 * to null). Java's manastone lists, cleanup, getItemTemplates, size, getManastones and getAncientManastones come with the P4-09 port (header
 * requests templates-b-1 and items-1 added getItemTemplate).
 *
 * @author Luno
 */
class ItemData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/ItemData.xml.inc"
private:
	std::unordered_map<int32_t, const model::templates::item::ItemTemplate*> items;

public:
	/** @return the item template, nullptr (Java null) for an unknown id */
	const model::templates::item::ItemTemplate* getItemTemplate(int32_t itemId) const;
};

} // namespace aion::gameserver::dataholders
