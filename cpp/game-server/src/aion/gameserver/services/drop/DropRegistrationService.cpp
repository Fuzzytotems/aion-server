#include "aion/gameserver/services/drop/DropRegistrationService.h"

#include "aion/gameserver/model/drop/DropItem.h"
#include "aion/gameserver/model/drop/DropModifiers.h"
#include "aion/gameserver/model/gameobjects/DropNpc.h"
#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::services::drop {

DropRegistrationService::DropRegistrationService() = default;

DropRegistrationService::~DropRegistrationService() = default;

DropRegistrationService& DropRegistrationService::getInstance() {
	static DropRegistrationService instance; // Java SingletonHolder
	return instance;
}

void DropRegistrationService::registerDrop(model::gameobjects::Npc& npc, model::gameobjects::player::Player& player, const std::vector<runtime::Ptr<model::gameobjects::player::Player>>& groupMembers) {
	AION_UNPORTED();
}

void DropRegistrationService::registerDrop(model::gameobjects::Npc& npc, model::gameobjects::player::Player& player, int32_t highestLevel, const std::vector<runtime::Ptr<model::gameobjects::player::Player>>& groupMembers) {
	AION_UNPORTED();
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
