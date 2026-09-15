#pragma once

#include <cstdint>
#include <map>
#include <unordered_map>

#include "aion/gameserver/dataholders/ItemRandomBonusData.xml.h"

namespace aion::gameserver::dataholders {

/**
 * Java com.aionemu.gameserver.dataholders.ItemRandomBonusData.
 * <p>
 * C++: the @XmlTransient index (Java EnumMap<StatBonusType, Map<Integer, RandomBonusSet>>) points into the bound `randomBonusSets` storage,
 * which stays after afterUnmarshal (static-data.md §2.6; Java sets the list to null). areBonusSetsEqual, selectRandomBonusNumber and size come
 * with the P4-09 port (header request items-2 added getTemplate).
 *
 * @author Rolandas
 */
class ItemRandomBonusData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/ItemRandomBonusData.xml.inc"
private:
	std::map<model::templates::item::bonuses::StatBonusType, std::unordered_map<int32_t, const model::templates::item::bonuses::RandomBonusSet*>>
	  bonusData;

	const model::templates::item::bonuses::RandomBonusSet* getBonusSet(model::templates::item::bonuses::StatBonusType statBonusType,
		int32_t statBonusSetId) const;

public:
	/**
	 * @return modifier set statBonusId (1-based) of the bonus set, nullptr (Java null) if the set does not exist
	 * @throws IndexOutOfBoundsException if the set has no modifier set statBonusId (Java List.get)
	 */
	const model::templates::stats::ModifiersTemplate* getTemplate(model::templates::item::bonuses::StatBonusType statBonusType,
		int32_t statBonusSetId, int32_t statBonusId) const;
};

} // namespace aion::gameserver::dataholders
