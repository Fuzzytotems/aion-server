#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

#include "aion/gameserver/dataholders/TemperingData.xml.h"
#include "aion/gameserver/model/templates/item/fwd.h"

namespace aion::gameserver::dataholders {

/**
 * Java com.aionemu.gameserver.dataholders.TemperingData.
 * <p>
 * C++: the @XmlTransient maps point into the bound `temperingList` storage, which stays after afterUnmarshal (static-data.md §2.6; Java sets the
 * list to null). Header request items-4 added getTemplates.
 *
 * @author xTz
 */
class TemperingData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/TemperingData.xml.inc"
private:
	/** Java Map<String, Map<Integer, List<TemperingStat>>> templates: item group or tempering name -> tempering level -> stats */
	std::map<std::string, std::unordered_map<int32_t, const std::vector<model::enchants::TemperingStat>*>, std::less<>> templates;

public:
	/**
	 * @return the stats per tempering level for the item's tempering name, or for its item group if it has none; nullptr (Java null) if there are
	 *         none
	 * @throws NullPointerException if itemTemplate is null
	 */
	const std::unordered_map<int32_t, const std::vector<model::enchants::TemperingStat>*>*
	getTemplates(const model::templates::item::ItemTemplate* itemTemplate) const;

	int32_t size() const;
};

} // namespace aion::gameserver::dataholders
