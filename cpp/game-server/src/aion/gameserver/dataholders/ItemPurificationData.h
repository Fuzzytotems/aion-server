#pragma once

#include <cstdint>
#include <unordered_map>

#include "aion/gameserver/dataholders/ItemPurificationData.xml.h"

namespace aion::gameserver::dataholders {

/**
 * Java com.aionemu.gameserver.dataholders.ItemPurificationData.
 * <p>
 * C++: the indexes point into the bound `itemPurificationTemplates` storage, which stays after afterUnmarshal (static-data.md §2.6).
 *
 * @author Ranastic, Navyan
 */
class ItemPurificationData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/ItemPurificationData.xml.inc"
public:
	using ResultItemMap = std::unordered_map<int32_t, const model::templates::item::purification::PurificationResult*>;

private:
	std::unordered_map<int32_t, const model::templates::item::purification::ItemPurificationTemplate*> itemPurificationSets;
	std::unordered_map<int32_t, ResultItemMap> possibleResultItems;

public:
	/** @return the purification template of the base item, nullptr (Java null) if there is none */
	const model::templates::item::purification::ItemPurificationTemplate* getItemPurificationTemplate(int32_t itemSetId) const;

	/** @return the purification results by result item id, nullptr (Java null) if the base item has none */
	const ResultItemMap* getResultItemMap(int32_t baseItemId) const;

	int32_t size() const;
};

} // namespace aion::gameserver::dataholders
