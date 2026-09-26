#include "aion/gameserver/model/team/common/legacy/LootGroupRules.h"

#include <utility>

#include "aion/gameserver/model/drop/DropItem.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/team/common/legacy/LootRuleType.h"
#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::model::team::common::legacy {

LootGroupRules::LootGroupRules()
	: lootRule(LootRuleType::ROUNDROBIN), misc(0), commonItemAbove(0), superiorItemAbove(2), heroicItemAbove(2), fabledItemAbove(2),
	  eternalItemAbove(2), mythicItemAbove(2) {
}

LootGroupRules::LootGroupRules(LootRuleType lootRuleValue, int32_t miscValue, int32_t commonItemAboveValue, int32_t superiorItemAboveValue,
	int32_t heroicItemAboveValue, int32_t fabledItemAboveValue, int32_t eternalItemAboveValue, int32_t mythicItemAboveValue)
	: lootRule(lootRuleValue), misc(miscValue), commonItemAbove(commonItemAboveValue), superiorItemAbove(superiorItemAboveValue),
	  heroicItemAbove(heroicItemAboveValue), fabledItemAbove(fabledItemAboveValue), eternalItemAbove(eternalItemAboveValue),
	  mythicItemAbove(mythicItemAboveValue) {
}

LootGroupRules::~LootGroupRules() = default;

runtime::Ref<LootGroupRules> LootGroupRules::create() {
	return runtime::makeRef<LootGroupRules>();
}

runtime::Ref<LootGroupRules> LootGroupRules::create(LootRuleType lootRuleValue, int32_t miscValue, int32_t commonItemAboveValue,
	int32_t superiorItemAboveValue, int32_t heroicItemAboveValue, int32_t fabledItemAboveValue, int32_t eternalItemAboveValue,
	int32_t mythicItemAboveValue) {
	return runtime::makeRef<LootGroupRules>(lootRuleValue, miscValue, commonItemAboveValue, superiorItemAboveValue, heroicItemAboveValue,
		fabledItemAboveValue, eternalItemAboveValue, mythicItemAboveValue);
}

bool LootGroupRules::getQualityRule(templates::item::ItemQuality quality) {
	AION_UNPORTED();
}

bool LootGroupRules::isMisc(templates::item::ItemQuality quality) {
	AION_UNPORTED();
}

int32_t LootGroupRules::getAutodistributionId() {
	AION_UNPORTED();
}

void LootGroupRules::setPlayersInRoll(std::vector<runtime::Ref<gameobjects::player::Player>> players, int32_t time, int32_t index, int32_t npcId) {
	AION_UNPORTED();
}

void LootGroupRules::addItemToBeDistributed(drop::DropItem& dropItem) {
	AION_UNPORTED();
}

bool LootGroupRules::containDropItem(drop::DropItem& dropItem) {
	AION_UNPORTED();
}

void LootGroupRules::removeItemToBeDistributed(drop::DropItem& dropItem) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::model::team::common::legacy
