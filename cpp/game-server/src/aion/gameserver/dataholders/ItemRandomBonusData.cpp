#include "aion/gameserver/dataholders/ItemRandomBonusData.h"

#include <string>

#include "aion/gameserver/runtime/base/Exceptions.h"

namespace aion::gameserver::dataholders {

using model::templates::item::bonuses::RandomBonusSet;
using model::templates::item::bonuses::StatBonusType;

void ItemRandomBonusData::afterUnmarshal(xml::LoadContext& /*ctx*/, const xml::XmlParent& /*parent*/) {
	// Java: bonusData.put(statBonusType, new HashMap<>()) for every StatBonusType; a missing key of the C++ map reads as that empty map
	bonusData.clear();
	for (const RandomBonusSet& bonus : randomBonusSets)
		bonusData[bonus.getBonusType()].insert_or_assign(bonus.getId(), &bonus);
	// Java: randomBonusSets = null (the C++ index points into the storage, which stays)
}

const RandomBonusSet* ItemRandomBonusData::getBonusSet(StatBonusType statBonusType, int32_t statBonusSetId) const {
	auto sets = bonusData.find(statBonusType);
	if (sets == bonusData.end())
		return nullptr;
	auto it = sets->second.find(statBonusSetId);
	return it != sets->second.end() ? it->second : nullptr;
}

const model::templates::stats::ModifiersTemplate* ItemRandomBonusData::getTemplate(StatBonusType statBonusType, int32_t statBonusSetId,
	int32_t statBonusId) const {
	const RandomBonusSet* bonus = getBonusSet(statBonusType, statBonusSetId);
	if (bonus == nullptr)
		return nullptr;
	const auto& modifiers = bonus->getModifiers();
	int64_t index = static_cast<int64_t>(statBonusId) - 1;
	if (index < 0 || index >= static_cast<int64_t>(modifiers.size()))
		throw runtime::IndexOutOfBoundsException("Index " + std::to_string(index) + " out of bounds for length " + std::to_string(modifiers.size()));
	return &modifiers[static_cast<size_t>(index)];
}

} // namespace aion::gameserver::dataholders
