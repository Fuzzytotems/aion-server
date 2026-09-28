#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

#include "aion/gameserver/dataholders/EnchantData.xml.h"
#include "aion/gameserver/model/templates/item/fwd.h"

namespace aion::gameserver::dataholders {

/**
 * Java com.aionemu.gameserver.dataholders.EnchantData.
 * <p>
 * C++: the level maps point into the bound `enchantList` storage, which stays after afterUnmarshal (static-data.md §2.6).
 *
 * @author xTz
 */
class EnchantData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/EnchantData.xml.inc"
public:
	/** Java: Map<Integer, List<EnchantStat>> (enchant level -> stats) */
	using LevelStats = std::unordered_map<int32_t, const std::vector<model::enchants::EnchantStat>*>;

private:
	std::map<std::string, LevelStats, std::less<>> templates;

public:
	int32_t size() const;

	/** @return the enchant stats per level of the item's enchant name (or item group name), nullptr (Java null) if there are none */
	const LevelStats* getTemplates(const model::templates::item::ItemTemplate& itemTemplate) const;
};

} // namespace aion::gameserver::dataholders
