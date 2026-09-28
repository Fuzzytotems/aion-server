#include "aion/gameserver/model/stats/calc/functions/StatArmorMasteryFunction.h"

#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/items/ItemSlot.h"
#include "aion/gameserver/model/items/ItemSlotInfo.h"
#include "aion/gameserver/model/stats/calc/Stat2.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/model/templates/item/enums/ItemSubType.h"

namespace aion::gameserver::model::stats::calc::functions {

StatArmorMasteryFunction::StatArmorMasteryFunction(templates::item::enums::ItemSubType armorTypeValue, container::StatEnum name, int32_t valueValue,
	bool bonusValue, int32_t fixedBonusValue, const std::vector<runtime::Ptr<gameobjects::Item>>& equipment)
	: StatRateFunction(name, valueValue, bonusValue), armorType(armorTypeValue), fixedBonus(fixedBonusValue) {
	updateEquipmentFactor(equipment);
}

StatArmorMasteryFunction::~StatArmorMasteryFunction() = default;

runtime::Ref<StatArmorMasteryFunction> StatArmorMasteryFunction::create(templates::item::enums::ItemSubType armorTypeValue, container::StatEnum name,
	int32_t valueValue, bool bonusValue, int32_t fixedBonusValue, const std::vector<runtime::Ptr<gameobjects::Item>>& equipment) {
	return runtime::makeRef<StatArmorMasteryFunction>(armorTypeValue, name, valueValue, bonusValue, fixedBonusValue, equipment);
}

void StatArmorMasteryFunction::updateEquipmentFactor(const std::vector<runtime::Ptr<gameobjects::Item>>& equipment) {
	// java-race: equipmentFactor is reset and summed without synchronization, as in Java (a concurrent reader may see a partial sum)
	equipmentFactor = 0;
	for (const runtime::Ptr<gameobjects::Item>& item : equipment) {
		if (item->getItemTemplate()->getItemSubType() == armorType) {
			equipmentFactor = equipmentFactor.get() + getEquipmentFactor(items::getSlotFor(item->getEquipmentSlot()));
		}
	}
}

int32_t StatArmorMasteryFunction::getEquipmentFactor(items::ItemSlot itemSlot) {
	switch (itemSlot) {
		case items::ItemSlot::TORSO:
			return 30;
		case items::ItemSlot::PANTS:
			return 25;
		case items::ItemSlot::SHOULDER:
		case items::ItemSlot::GLOVES:
		case items::ItemSlot::BOOTS:
			return 15;
		default:
			return 0;
	}
}

void StatArmorMasteryFunction::apply(Stat2& statValue, const std::unordered_set<utils::stats::CalculationType>& calculationTypes) {
	StatRateFunction::apply(statValue, calculationTypes);
	if (fixedBonus != 0 && equipmentFactor.get() != 0)
		statValue.addToBonus(fixedBonus * equipmentFactor.get() / 100.0f);
}

int32_t StatArmorMasteryFunction::getValue() {
	return value * equipmentFactor.get() / 100; // truncation from equipmentFactor is retail-like
}

} // namespace aion::gameserver::model::stats::calc::functions
