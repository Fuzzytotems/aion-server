#include "aion/gameserver/model/team/common/legacy/LootGroupRules.h"

#include <memory>
#include <utility>

#include "aion/gameserver/model/drop/DropItem.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/team/common/legacy/LootRuleType.h"
#include "aion/gameserver/model/actions/PlayerMode.h"
#include "aion/gameserver/model/gameobjects/player/InRoll.h"
#include "aion/gameserver/model/templates/item/ItemQuality.h"
#include "aion/gameserver/runtime/sched/TaskConcepts.h"
#include "aion/gameserver/services/drop/DropDistributionService.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"

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
	using templates::item::ItemQuality;
	switch (quality) {
		case ItemQuality::COMMON:
			return commonItemAbove != 0; // White
		case ItemQuality::RARE:
			return superiorItemAbove != 0; // Green
		case ItemQuality::LEGEND:
			return heroicItemAbove != 0; // Blue
		case ItemQuality::UNIQUE:
			return fabledItemAbove != 0; // Yellow
		case ItemQuality::EPIC:
			return eternalItemAbove != 0; // Orange
		case ItemQuality::MYTHIC:
			return mythicItemAbove != 0; // Purple
		default:
			return false;
	}
}

bool LootGroupRules::isMisc(templates::item::ItemQuality quality) {
	return quality == templates::item::ItemQuality::JUNK && misc == 1;
}

int32_t LootGroupRules::getAutodistributionId() {
	bool isBid = mythicItemAbove == 3;
	bool isRoll = mythicItemAbove == 2;
	return isBid ? 3 : isRoll ? 2 : 0;
}

// lambda at LootGroupRules.java:113 (a task capturing the players, the index and the npc id): an unpinned bindTask with the Refs the caller
// handed over in a shared immutable vector (a TaskArg; the class comment: the task keeps them alive until it runs, like Java's captured collection)
void LootGroupRules::setPlayersInRoll(std::vector<runtime::Ref<gameobjects::player::Player>> players, int32_t time, int32_t index, int32_t npcId) {
	utils::ThreadPoolManager::getInstance().schedule(
		runtime::bindTask(
			[](const std::shared_ptr<const std::vector<runtime::Ref<gameobjects::player::Player>>>& rollPlayers, int32_t rollIndex, int32_t rollNpcId) {
				for (const runtime::Ref<gameobjects::player::Player>& player : *rollPlayers) {
					if (player->isInPlayerMode(actions::PlayerMode::IN_ROLL)) {
						runtime::Ptr<gameobjects::player::InRoll> inRoll = player->inRoll.get();
						if (inRoll->getIndex() == rollIndex && inRoll->getNpcId() == rollNpcId)
							services::drop::DropDistributionService::getInstance().handleRollOrBid(runtime::Ptr<gameobjects::player::Player>(player),
								inRoll->getRollType(), 0, 0, inRoll->getItemId(), inRoll->getNpcId(), inRoll->getIndex());
					}
				}
			},
			std::make_shared<const std::vector<runtime::Ref<gameobjects::player::Player>>>(std::move(players)), index, npcId),
		time);
}

void LootGroupRules::addItemToBeDistributed(drop::DropItem& dropItem) {
	itemsToBeDistributed.add(runtime::Ref<drop::DropItem>(dropItem));
}

bool LootGroupRules::containDropItem(drop::DropItem& dropItem) {
	return itemsToBeDistributed.contains(runtime::Ptr<drop::DropItem>(dropItem));
}

void LootGroupRules::removeItemToBeDistributed(drop::DropItem& dropItem) {
	itemsToBeDistributed.remove(runtime::Ptr<drop::DropItem>(dropItem));
}

} // namespace aion::gameserver::model::team::common::legacy
