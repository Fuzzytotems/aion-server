#include "aion/gameserver/dataholders/ItemRandomBonusData.h"

#include <string>

#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/runtime/base/Exceptions.h"

namespace aion::gameserver::dataholders {

using model::templates::item::bonuses::RandomBonusSet;
using model::templates::item::bonuses::StatBonusType;
using model::templates::stats::ModifiersTemplate;

void ItemRandomBonusData::afterUnmarshal(xml::LoadContext& /*ctx*/, const xml::XmlParent& /*parent*/) {
	// Java: bonusData.put(statBonusType, new HashMap<>()) for every StatBonusType; a missing key of the C++ map reads as that empty map
	bonusData.clear();
	for (const RandomBonusSet& bonus : randomBonusSets)
		bonusData[bonus.getBonusType()].insert_or_assign(bonus.getId(), &bonus);
	// Java: randomBonusSets = null (the C++ index points into the storage, which stays)
}

bool ItemRandomBonusData::areBonusSetsEqual(StatBonusType statBonusType, int32_t statBonusSetId1, int32_t statBonusSetId2) const {
	if (statBonusSetId1 == statBonusSetId2)
		return true;
	const RandomBonusSet* bonusSet1 = getBonusSet(statBonusType, statBonusSetId1);
	const RandomBonusSet* bonusSet2 = getBonusSet(statBonusType, statBonusSetId2);
	if (bonusSet1 == nullptr || bonusSet2 == nullptr)
		return bonusSet1 == bonusSet2;
	// if size comparison isn't sufficient we should override ModifiersTemplate.equals() and check if both lists are equal
	return bonusSet1->getModifiers().size() == bonusSet2->getModifiers().size();
}

const RandomBonusSet* ItemRandomBonusData::getBonusSet(StatBonusType statBonusType, int32_t statBonusSetId) const {
	auto sets = bonusData.find(statBonusType);
	if (sets == bonusData.end())
		return nullptr;
	auto it = sets->second.find(statBonusSetId);
	return it != sets->second.end() ? it->second : nullptr;
}

int32_t ItemRandomBonusData::selectRandomBonusNumber(StatBonusType statBonusType, int32_t statBonusSetId) const {
	const RandomBonusSet* bonus = getBonusSet(statBonusType, statBonusSetId);
	if (bonus == nullptr)
		return 0;

	const std::vector<ModifiersTemplate>& modifiersGroup = bonus->getModifiers();
	float chance = commons::utils::Rnd::nextFloat(calculateSumOfChances(modifiersGroup));
	float chanceSum = 0;
	for (size_t i = 0; i < modifiersGroup.size(); i++) {
		chanceSum += modifiersGroup[i].getChance();
		if (chanceSum >= chance)
			return static_cast<int32_t>(i) + 1;
	}
	return 0;
}

float ItemRandomBonusData::calculateSumOfChances(const std::vector<ModifiersTemplate>& modifiersGroup) {
	float sumOfAllChances = 0;
	for (const ModifiersTemplate& modifiersTemplate : modifiersGroup)
		sumOfAllChances += modifiersTemplate.getChance();
	return sumOfAllChances;
}

const ModifiersTemplate* ItemRandomBonusData::getTemplate(StatBonusType statBonusType, int32_t statBonusSetId, int32_t statBonusId) const {
	const RandomBonusSet* bonus = getBonusSet(statBonusType, statBonusSetId);
	if (bonus == nullptr)
		return nullptr;
	const auto& modifiers = bonus->getModifiers();
	int64_t index = static_cast<int64_t>(statBonusId) - 1;
	if (index < 0 || index >= static_cast<int64_t>(modifiers.size()))
		throw runtime::IndexOutOfBoundsException("Index " + std::to_string(index) + " out of bounds for length " + std::to_string(modifiers.size()));
	return &modifiers[static_cast<size_t>(index)];
}

int32_t ItemRandomBonusData::size() const {
	int32_t sum = 0;
	for (const auto& [type, sets] : bonusData)
		sum += static_cast<int32_t>(sets.size());
	return sum;
}

} // namespace aion::gameserver::dataholders
