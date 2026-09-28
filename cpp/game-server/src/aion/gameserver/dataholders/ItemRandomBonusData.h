#pragma once

#include <cstdint>
#include <map>
#include <unordered_map>
#include <vector>

#include "aion/gameserver/dataholders/ItemRandomBonusData.xml.h"

namespace aion::gameserver::dataholders {

/**
 * Java com.aionemu.gameserver.dataholders.ItemRandomBonusData.
 * <p>
 * C++: the @XmlTransient index (Java EnumMap<StatBonusType, Map<Integer, RandomBonusSet>>) points into the bound `randomBonusSets` storage,
 * which stays after afterUnmarshal (static-data.md §2.6; Java sets the list to null).
 *
 * @author Rolandas
 */
class ItemRandomBonusData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/ItemRandomBonusData.xml.inc"
private:
	std::map<model::templates::item::bonuses::StatBonusType, std::unordered_map<int32_t, const model::templates::item::bonuses::RandomBonusSet*>>
	  bonusData;

public:
	/**
	 * NCSoft in their wisdom decided to implement different stat bonus sets with identical content and use one for the base item and the other one
	 * for the purified version of that item. To ensure that purification does not trigger a re-roll of random bonus stats, we need this method
	 */
	bool areBonusSetsEqual(model::templates::item::bonuses::StatBonusType statBonusType, int32_t statBonusSetId1, int32_t statBonusSetId2) const;

private:
	const model::templates::item::bonuses::RandomBonusSet* getBonusSet(model::templates::item::bonuses::StatBonusType statBonusType,
	                                                                   int32_t statBonusSetId) const;

public:
	int32_t selectRandomBonusNumber(model::templates::item::bonuses::StatBonusType statBonusType, int32_t statBonusSetId) const;

private:
	static float calculateSumOfChances(const std::vector<model::templates::stats::ModifiersTemplate>& modifiersGroup);

public:
	/**
	 * @return modifier set statBonusId (1-based) of the bonus set, nullptr (Java null) if the set does not exist
	 * @throws IndexOutOfBoundsException if the set has no modifier set statBonusId (Java List.get)
	 */
	const model::templates::stats::ModifiersTemplate* getTemplate(model::templates::item::bonuses::StatBonusType statBonusType, int32_t statBonusSetId,
	                                                              int32_t statBonusId) const;

	int32_t size() const;
};

} // namespace aion::gameserver::dataholders
