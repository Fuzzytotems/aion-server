#include "aion/gameserver/services/drop/DropRegistrationService.h"

#include "aion/gameserver/model/drop/DropItem.h"
#include "aion/gameserver/model/drop/DropModifiers.h"
#include "aion/gameserver/model/gameobjects/DropNpc.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::services::drop {

DropRegistrationService::DropRegistrationService() = default;

DropRegistrationService::~DropRegistrationService() = default;

DropRegistrationService& DropRegistrationService::getInstance() {
	static DropRegistrationService instance; // Java SingletonHolder
	return instance;
}

void DropRegistrationService::registerDrop(model::gameobjects::Npc& npc, model::gameobjects::player::Player& player, const std::vector<runtime::Ptr<model::gameobjects::player::Player>>& groupMembers) {
	registerDrop(npc, player, player.getLevel(), groupMembers); // DropRegistrationService.java:52-54
}

void DropRegistrationService::registerDrop(model::gameobjects::Npc& npc, model::gameobjects::player::Player& player, int32_t highestLevel, const std::vector<runtime::Ptr<model::gameobjects::player::Player>>& groupMembers) {
	// Loot is M5b-3 (m5b-plan.md D5/O-05, docs/deviations/P5-09.md). Java (DropRegistrationService.java:59-109) builds the drop set from
	// CUSTOM_NPC_DROP, QuestService.getQuestDrop and the global-rule evaluator, puts it into currentDropMap (:80), registers the DropNpc with its
	// allowed looters through initDropNpc (:68), then calls instanceHandler.onDropRegistered and the DROP_REGISTERED AI event (:101-102), sends
	// SM_LOOT_STATUS(npcObjId, LOOT_ENABLE) to every allowed looter (:104-106) and schedules the free-for-all timer (:108). Every one of those is
	// behind a body this milestone does not have: 123 AION_UNPORTED sites across DropRegistrationService, DropService and DropDistributionService.
	//
	// Whole body, not an arm, because there is no partial answer that is Java-exact: a drop set built from ported predicates only would be a drop
	// set Java never computes, and sending LOOT_ENABLE for a corpse whose currentDropMap entry does not exist makes CM_START_LOOT answer with an
	// empty list. So the M5b-1 answer is "no drops at all", and the gate asserts the drop path by what NpcAI::ask(REWARD_LOOT) answered and by this
	// site's hit count (m5b-plan.md R3: exactly once per kill), not by an item.
	//
	// Why it must not throw: NpcController::onDie reaches this through doReward inside a try that only logs (NpcController.cpp:172-181, Java
	// NpcController.java:146-155), so an AION_UNPORTED here completed the kill and kept the experience but skipped InstanceHandler::onDie and the
	// DIED AI event, and logged an ERROR per kill (m5b-client-session.md S-1: five kills, five ERROR lines).
	//
	// What the skipped statements leave behind, and why nothing downstream trips over it: currentDropMap and dropRegistrationMap stay empty, so
	// NpcController::petLoot and findPetForLooting - which run AFTER the try, unguarded - find no DropNpc and return (NpcController.cpp:201-230),
	// and DropService::unregisterDrop on the despawn path removes nothing. No DropNpc is created at M5b-1 at all.
	AION_PARTIAL("npc drops are not registered yet (M5b-3)");
}

model::drop::DropModifiers DropRegistrationService::createDropModifiers(model::gameobjects::Npc& npc, model::gameobjects::player::Player& player, int32_t highestLevel) {
	AION_UNPORTED();
}

runtime::Ptr<model::gameobjects::player::Player> DropRegistrationService::initDropNpc(model::gameobjects::player::Player& player, int32_t npcObjId, std::vector<runtime::Ptr<model::gameobjects::player::Player>>& allowedLooters, const std::vector<runtime::Ptr<model::gameobjects::player::Player>>& groupMembers) {
	AION_UNPORTED();
}

bool DropRegistrationService::isAllowedDefaultGlobalDropNpc(model::gameobjects::Npc& npc, bool isChest) {
	AION_UNPORTED();
}

int32_t DropRegistrationService::addGlobalDrops(int32_t index, model::drop::DropModifiers& dropModifiers, model::gameobjects::player::Player& player, model::gameobjects::Npc& npc, bool isAllowedDefaultGlobalDropNpc, const std::vector<const model::templates::globaldrops::GlobalRule*>& rules, runtime::RcHashSet<runtime::Ref<model::drop::DropItem>>& droppedItems, const std::vector<runtime::Ptr<model::gameobjects::player::Player>>& groupMembers, int32_t winnerObj) {
	AION_UNPORTED();
}

std::optional<float> DropRegistrationService::getReductionDropRate(model::gameobjects::Npc& npc, int32_t highestLevel) {
	AION_UNPORTED();
}

float DropRegistrationService::calculateBoostDropRate(model::gameobjects::player::Player& killer, model::gameobjects::Npc& npc) {
	AION_UNPORTED();
}

float DropRegistrationService::calculateEffectiveChance(const model::templates::globaldrops::GlobalRule* rule, model::gameobjects::Npc& npc, model::drop::DropModifiers& dropModifiers) {
	AION_UNPORTED();
}

int32_t DropRegistrationService::addDropItems(int32_t index, runtime::RcHashSet<runtime::Ref<model::drop::DropItem>>& droppedItems, const model::templates::globaldrops::GlobalRule* rule, model::gameobjects::Npc& npc, model::gameobjects::player::Player& player, const std::vector<runtime::Ptr<model::gameobjects::player::Player>>& groupMembers, int32_t winnerObj, model::drop::DropModifiers& dropModifiers) {
	AION_UNPORTED();
}

runtime::Ref<model::drop::DropItem> DropRegistrationService::regDropItem(int32_t index, int32_t playerObjId, int32_t objId, int32_t itemId, int64_t value) {
	AION_UNPORTED();
}

bool DropRegistrationService::hasGlobalNpcExclusions(model::gameobjects::Npc& npc) {
	AION_UNPORTED();
}

bool DropRegistrationService::checkRuleRestrictions(const model::templates::globaldrops::GlobalRule* rule, model::Race race, model::gameobjects::Npc& npc) {
	AION_UNPORTED();
}

bool DropRegistrationService::checkRestrictionRace(const model::templates::globaldrops::GlobalRule* rule, model::Race race) {
	AION_UNPORTED();
}

bool DropRegistrationService::checkGlobalRuleMaps(const model::templates::globaldrops::GlobalRule* rule, model::gameobjects::Npc& npc) {
	AION_UNPORTED();
}

bool DropRegistrationService::checkGlobalRuleWorlds(const model::templates::globaldrops::GlobalRule* rule, model::gameobjects::Npc& npc) {
	AION_UNPORTED();
}

bool DropRegistrationService::checkGlobalRuleRatings(const model::templates::globaldrops::GlobalRule* rule, model::gameobjects::Npc& npc) {
	AION_UNPORTED();
}

bool DropRegistrationService::checkGlobalRuleRaces(const model::templates::globaldrops::GlobalRule* rule, model::gameobjects::Npc& npc) {
	AION_UNPORTED();
}

bool DropRegistrationService::checkGlobalRuleTribes(const model::templates::globaldrops::GlobalRule* rule, model::gameobjects::Npc& npc) {
	AION_UNPORTED();
}

bool DropRegistrationService::checkGlobalRuleZones(const model::templates::globaldrops::GlobalRule* rule, model::gameobjects::Npc& npc) {
	AION_UNPORTED();
}

bool DropRegistrationService::checkGlobalRuleNpcs(const model::templates::globaldrops::GlobalRule* rule, model::gameobjects::Npc& npc) {
	AION_UNPORTED();
}

bool DropRegistrationService::checkGlobalRuleNpcGroups(const model::templates::globaldrops::GlobalRule* rule, model::gameobjects::Npc& npc) {
	AION_UNPORTED();
}

bool DropRegistrationService::checkGlobalRuleExcludedNpcs(const model::templates::globaldrops::GlobalRule* rule, model::gameobjects::Npc& npc) {
	AION_UNPORTED();
}

std::vector<const model::templates::globaldrops::GlobalDropItem*> DropRegistrationService::collectDrops(const model::templates::globaldrops::GlobalRule* rule, model::gameobjects::Npc& npc, model::drop::DropModifiers& dropModifiers) {
	AION_UNPORTED();
}

std::vector<const model::templates::globaldrops::GlobalDropItem*> DropRegistrationService::collectAllowedDrops(const model::templates::globaldrops::GlobalRule* rule, model::gameobjects::Npc& npc, model::drop::DropModifiers& dropModifiers) {
	AION_UNPORTED();
}

int64_t DropRegistrationService::getItemCount(const model::templates::globaldrops::GlobalDropItem* item, model::gameobjects::Npc& npc) {
	AION_UNPORTED();
}

float DropRegistrationService::getRankModifier(model::gameobjects::Npc& npc) {
	AION_UNPORTED();
}

float DropRegistrationService::getRatingModifier(model::gameobjects::Npc& npc) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::services::drop
