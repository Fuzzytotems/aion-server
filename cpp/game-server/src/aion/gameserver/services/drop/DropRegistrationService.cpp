#include "aion/gameserver/services/drop/DropRegistrationService.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <limits>
#include <string>
#include <string_view>

#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/ai/AbstractAI.h"
#include "aion/gameserver/ai/event/AIEventType.h"
#include "aion/gameserver/configs/main/RatesConfig.h"
#include "aion/gameserver/dataholders/CustomDrop.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/GlobalDropData.h"
#include "aion/gameserver/dataholders/GlobalNpcExclusionData.h"
#include "aion/gameserver/dataholders/ItemData.h"
#include "aion/gameserver/dataholders/loadingutils/EnumTraits.h"
#include "aion/gameserver/instance/handlers/InstanceHandler.h"
#include "aion/gameserver/model/Chance.h"
#include "aion/gameserver/model/Race.h"
#include "aion/gameserver/model/drop/Drop.h"
#include "aion/gameserver/model/drop/DropItem.h"
#include "aion/gameserver/model/drop/DropModifiers.h"
#include "aion/gameserver/model/drop/NpcDrop.h"
#include "aion/gameserver/model/gameobjects/DropNpc.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"
#include "aion/gameserver/model/gameobjects/player/RatesInfo.h"
#include "aion/gameserver/model/house/House.h"
#include "aion/gameserver/model/items/ItemId.h"
#include "aion/gameserver/model/stats/calc/Stat2.h"
#include "aion/gameserver/model/stats/container/NpcGameStats.h"
#include "aion/gameserver/model/stats/container/PlayerGameStats.h"
#include "aion/gameserver/model/stats/container/StatEnum.h"
#include "aion/gameserver/model/team/TemporaryPlayerTeam.h"
#include "aion/gameserver/model/team/alliance/PlayerAlliance.h"
#include "aion/gameserver/model/team/common/legacy/LootGroupRules.h"
#include "aion/gameserver/model/team/common/legacy/LootRuleType.h"
#include "aion/gameserver/model/team/group/PlayerGroup.h"
#include "aion/gameserver/model/templates/globaldrops/GlobalDropExcludedNpcs.h"
#include "aion/gameserver/model/templates/globaldrops/GlobalDropItem.h"
#include "aion/gameserver/model/templates/globaldrops/GlobalDropMap.h"
#include "aion/gameserver/model/templates/globaldrops/GlobalDropMaps.h"
#include "aion/gameserver/model/templates/globaldrops/GlobalDropNpc.h"
#include "aion/gameserver/model/templates/globaldrops/GlobalDropNpcGroup.h"
#include "aion/gameserver/model/templates/globaldrops/GlobalDropNpcGroups.h"
#include "aion/gameserver/model/templates/globaldrops/GlobalDropNpcs.h"
#include "aion/gameserver/model/templates/globaldrops/GlobalDropRace.h"
#include "aion/gameserver/model/templates/globaldrops/GlobalDropRaces.h"
#include "aion/gameserver/model/templates/globaldrops/GlobalDropRating.h"
#include "aion/gameserver/model/templates/globaldrops/GlobalDropRatings.h"
#include "aion/gameserver/model/templates/globaldrops/GlobalDropTribe.h"
#include "aion/gameserver/model/templates/globaldrops/GlobalDropTribes.h"
#include "aion/gameserver/model/templates/globaldrops/GlobalDropWorld.h"
#include "aion/gameserver/model/templates/globaldrops/GlobalDropWorlds.h"
#include "aion/gameserver/model/templates/globaldrops/GlobalDropZone.h"
#include "aion/gameserver/model/templates/globaldrops/GlobalDropZones.h"
#include "aion/gameserver/model/templates/globaldrops/GlobalRule.h"
#include "aion/gameserver/model/templates/housing/HouseType.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/model/templates/npc/AbyssNpcType.h"
#include "aion/gameserver/model/templates/npc/NpcRank.h"
#include "aion/gameserver/model/templates/npc/NpcRating.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/model/templates/spawns/basespawns/BaseSpawnTemplate.h"
#include "aion/gameserver/model/templates/spawns/siegespawns/SiegeSpawnTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_LOOT_STATUS.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/services/QuestService.h"
#include "aion/gameserver/services/drop/DropService.h"
#include "aion/gameserver/services/event/EventService.h"
#include "aion/gameserver/spawnengine/SpawnHandlerType.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/utils/stats/DropRewardEnumInfo.h"
#include "aion/gameserver/world/WorldDropType.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldMapType.h"
#include "aion/gameserver/world/WorldMapTypeInfo.h"
#include "aion/gameserver/world/WorldPosition.h"
#include "aion/gameserver/world/zone/ZoneName.h"

namespace aion::gameserver::services::drop {

namespace {

using model::gameobjects::Npc;
using model::gameobjects::player::Player;
using model::templates::globaldrops::GlobalDropItem;
using model::templates::globaldrops::GlobalRule;

/** Java: (long) a - NaN 0, saturating (the narrowing of a compound assignment `long *= double`, JLS 15.26.2) */
int64_t javaLongCast(double a) noexcept {
	if (a != a)
		return 0;
	if (a >= 9223372036854775808.0)
		return std::numeric_limits<int64_t>::max();
	if (a <= -9223372036854775808.0)
		return std::numeric_limits<int64_t>::min();
	return static_cast<int64_t>(a);
}

/** Java: String.toLowerCase() of an enum constant's name (ASCII) */
std::string toLowerCase(std::string_view value) {
	std::string lower(value);
	std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	return lower;
}

/** Java: DataManager.GLOBAL_DROP_DATA.getAllRules() - the rules of the static data in their order (the holder keeps them by value) */
std::vector<const GlobalRule*> allRulesOf(const dataholders::GlobalDropData& globalDropData) {
	std::vector<const GlobalRule*> rules;
	rules.reserve(globalDropData.getAllRules().size());
	for (const GlobalRule& rule : globalDropData.getAllRules())
		rules.push_back(&rule);
	return rules;
}

} // namespace

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
	// C++: an empty groupMembers stands for Java null (m5b3-plan.md D11): NpcController.doReward passes null for a solo kill
	// (NpcController.cpp:277-279); the callees agree (NpcDrop/DropGroup, QuestService.getQuestDrop, addDropItems)
	int32_t npcObjId = npc.getObjectId();

	// Getting all possible drops for this Npc
	const model::drop::NpcDrop* npcDrop = dataholders::DataManager::CUSTOM_NPC_DROP->getNpcDrop(npc.getNpcId());

	std::vector<runtime::Ptr<Player>> allowedLooters;
	runtime::Ptr<Player> looter = player;
	int32_t winnerObj = 0;
	runtime::Ptr<Player> teamLooter = initDropNpc(player, npcObjId, allowedLooters, groupMembers);
	if (teamLooter) {
		looter = teamLooter;
		winnerObj = teamLooter->getObjectId();
	}

	int32_t index = 1;
	// Java: new HashSet<>(); its monitor is the `synchronized (dropItems)` of DropService.requestDropItem (DropService.java:286, :393)
	runtime::Ref<runtime::RcHashSet<runtime::Ref<model::drop::DropItem>>> droppedItems =
		runtime::RcHashSet<runtime::Ref<model::drop::DropItem>>::create(AION_LOCK_CLASS(DropRegistrationService::currentDropMap#dropItems));
	model::drop::DropModifiers dropModifiers = createDropModifiers(npc, *looter, highestLevel);

	if (npcDrop != nullptr) // add custom drops
		index = npcDrop->dropCalculator(*droppedItems, index, dropModifiers, groupMembers);

	// Updating current dropMap
	currentDropMap.put(npcObjId, droppedItems);

	index = QuestService::getQuestDrop(*droppedItems, index, npc, groupMembers, *looter);

	// if npc ai == quest_use_item it will be always excluded from global drops
	bool isNpcQuest = npc.getAi().getName() == "quest_use_item";
	if (!isNpcQuest) {
		bool hasGlobalNpcExclusions = this->hasGlobalNpcExclusions(npc);
		bool isAllowedDefaultGlobalDropNpc = this->isAllowedDefaultGlobalDropNpc(npc, dropModifiers.isDropNpcChest());
		// instances with WorldDropType.NONE must not have global drops (example Arenas)
		if (!hasGlobalNpcExclusions && npc.getWorldDropType() != world::WorldDropType::NONE) {
			index = addGlobalDrops(index, dropModifiers, *looter, npc, isAllowedDefaultGlobalDropNpc, allRulesOf(*dataholders::DataManager::GLOBAL_DROP_DATA),
				*droppedItems, groupMembers, winnerObj);
		}
		if (!hasGlobalNpcExclusions || dropModifiers.isDropNpcChest())
			addGlobalDrops(index, dropModifiers, *looter, npc, isAllowedDefaultGlobalDropNpc, event::EventService::getInstance().getActiveEventDropRules()->snapshot(),
				*droppedItems, groupMembers, winnerObj);
	}

	npc.getPosition()->getWorldMapInstance()->getInstanceHandler()->onDropRegistered(npc, winnerObj);
	npc.getAi().onGeneralEvent(ai::event::AIEventType::DROP_REGISTERED);

	for (const runtime::Ptr<Player>& p : allowedLooters) {
		utils::PacketSendUtility::sendPacket(*p, network::aion::serverpackets::SM_LOOT_STATUS(npcObjId, network::aion::serverpackets::SM_LOOT_STATUS::Status::LOOT_ENABLE));
	}

	DropService::getInstance().scheduleFreeForAll(npcObjId);
}

model::drop::DropModifiers DropRegistrationService::createDropModifiers(model::gameobjects::Npc& npc, model::gameobjects::player::Player& player, int32_t highestLevel) {
	model::drop::DropModifiers dropModifiers;
	std::string dropType = toLowerCase(xml::enumName(npc.getGroupDrop()));
	bool isChest = npc.getAi().getName() == "chest" || dropType.starts_with("treasure") || dropType.ends_with("box");
	dropModifiers.setIsDropNpcChest(isChest);
	dropModifiers.setDropRace(player.getRace());
	dropModifiers.setBoostDropRate(calculateBoostDropRate(player, npc));
	dropModifiers.setReductionDropRate(getReductionDropRate(npc, highestLevel));
	return dropModifiers;
}

runtime::Ptr<model::gameobjects::player::Player> DropRegistrationService::initDropNpc(model::gameobjects::player::Player& player, int32_t npcObjId, std::vector<runtime::Ptr<model::gameobjects::player::Player>>& allowedLooters, const std::vector<runtime::Ptr<model::gameobjects::player::Player>>& groupMembers) {
	using model::team::common::legacy::LootRuleType;
	runtime::Ptr<Player> looter = nullptr;
	runtime::Ref<model::gameobjects::DropNpc> dropNpc = model::gameobjects::DropNpc::create(npcObjId);
	// Distributing drops to players
	runtime::Ptr<model::team::TemporaryPlayerTeam> lootingTeam = player.getCurrentTeam();
	if (lootingTeam) {
		// C++: groupMembers is read as the caller's collection here. The one caller that passes Java null (NpcController.doReward's Player arm)
		// never has a team: a team member's damage is the team's (TeamDamageList), which goes to PlayerTeamDistributionService (M5g)
		runtime::Ptr<model::team::common::legacy::LootGroupRules> lootGroupRules = lootingTeam->getLootGroupRules();

		switch (lootGroupRules->getLootRule()) {
			case LootRuleType::ROUNDROBIN: {
				int32_t size = static_cast<int32_t>(groupMembers.size());
				if (size > lootGroupRules->getNrRoundRobin())
					lootGroupRules->setNrRoundRobin(lootGroupRules->getNrRoundRobin() + 1);
				else
					lootGroupRules->setNrRoundRobin(1);

				int32_t i = 0;
				for (const runtime::Ptr<Player>& p : groupMembers) {
					i++;
					if (i == lootGroupRules->getNrRoundRobin()) {
						allowedLooters.push_back(p);
						looter = p;
						break;
					}
				}
				break;
			}
			case LootRuleType::FREEFORALL:
				allowedLooters.insert(allowedLooters.end(), groupMembers.begin(), groupMembers.end());
				break;
			case LootRuleType::LEADER: {
				runtime::Ptr<Player> leader = player.isInGroup() ? player.getPlayerGroup()->getLeaderObject() : player.getPlayerAlliance()->getLeaderObject();
				allowedLooters.push_back(leader);
				looter = leader;
				break;
			}
		}
		// Java stores the caller's collection (DropNpc.setInRangePlayers); C++: a list with the same members
		runtime::Ref<runtime::RcArrayList<runtime::Ref<Player>>> inRangePlayers =
			runtime::RcArrayList<runtime::Ref<Player>>::create(AION_LOCK_CLASS(DropNpc::inRangePlayers));
		for (const runtime::Ptr<Player>& member : groupMembers)
			inRangePlayers->add(runtime::Ref<Player>(member));
		dropNpc->setInRangePlayers(inRangePlayers);
		dropNpc->setLootingTeam(*lootingTeam);
	} else {
		allowedLooters.push_back(player);
	}
	for (const runtime::Ptr<Player>& allowedLooter : allowedLooters)
		dropNpc->setAllowedLooter(*allowedLooter);
	dropRegistrationMap.put(npcObjId, dropNpc);
	return looter;
}

bool DropRegistrationService::isAllowedDefaultGlobalDropNpc(model::gameobjects::Npc& npc, bool isChest) {
	using model::templates::npc::AbyssNpcType;
	// exclude most siege spawns, and inner base spawns
	if (dynamic_cast<model::templates::spawns::siegespawns::SiegeSpawnTemplate*>(npc.getSpawn().get()) != nullptr && npc.getAbyssNpcType() != AbyssNpcType::DEFENDER)
		return false;
	if (dynamic_cast<model::templates::spawns::basespawns::BaseSpawnTemplate*>(npc.getSpawn().get()) != nullptr &&
		npc.getSpawn()->getHandlerType() != spawnengine::SpawnHandlerType::OUTRIDER && npc.getSpawn()->getHandlerType() != spawnengine::SpawnHandlerType::OUTRIDER_ENHANCED)
		return false;
	// if npc level == 1 means missing stats, so better exclude it from drops
	if (npc.getLevel() < 2 && !isChest && npc.getWorldId() != world::getId(world::WorldMapType::POETA) && npc.getWorldId() != world::getId(world::WorldMapType::ISHALGEN))
		return false;
	// if abyss type npc != null or npc is chest, the npc will be excluded from drops
	if (isChest || npc.getAbyssNpcType() != AbyssNpcType::NONE && npc.getAbyssNpcType() != AbyssNpcType::DEFENDER)
		return false;
	return true;
}

int32_t DropRegistrationService::addGlobalDrops(int32_t index, model::drop::DropModifiers& dropModifiers, model::gameobjects::player::Player& player, model::gameobjects::Npc& npc, bool isAllowedDefaultGlobalDropNpc, const std::vector<const model::templates::globaldrops::GlobalRule*>& rules, runtime::RcHashSet<runtime::Ref<model::drop::DropItem>>& droppedItems, const std::vector<runtime::Ptr<model::gameobjects::player::Player>>& groupMembers, int32_t winnerObj) {
	for (const GlobalRule* rule : rules) {
		// if getGlobalRuleNpcs() != null means drops are for specified npcs (like named drops) so the default restrictions will be ignored
		if (isAllowedDefaultGlobalDropNpc || rule->getGlobalRuleNpcs() != nullptr) {
			float chance = calculateEffectiveChance(rule, npc, dropModifiers);
			if (commons::utils::Rnd::chance() >= chance)
				continue;

			index = addDropItems(index, droppedItems, rule, npc, player, groupMembers, winnerObj, dropModifiers);
		}
	}
	return index;
}

std::optional<float> DropRegistrationService::getReductionDropRate(model::gameobjects::Npc& npc, int32_t highestLevel) {
	int32_t dropChance = utils::stats::dropRewardFrom(npc.getLevel() - highestLevel); // reduced chance depending on level
	return dropChance == 100 ? std::nullopt : std::optional<float>(static_cast<float>(dropChance) / 100.0f);
}

float DropRegistrationService::calculateBoostDropRate(model::gameobjects::player::Player& killer, model::gameobjects::Npc& npc) {
	using model::stats::container::StatEnum;
	// Drop rate from NPC can be boosted by Spiritmaster Erosion skill
	int32_t boostDropRate = npc.getGameStats()->getStat(StatEnum::BOOST_DROP_RATE, 100.0f)->getCurrent();
	// can be exploited on duel with Spiritmaster Erosion skill
	boostDropRate = killer.getGameStats()->getStat(StatEnum::BOOST_DROP_RATE, static_cast<float>(boostDropRate))->getCurrent();
	// Drop rate can be boosted by player buff too
	boostDropRate = killer.getGameStats()->getStat(StatEnum::DR_BOOST, static_cast<float>(boostDropRate))->getCurrent();

	if (killer.getCommonData()->getCurrentReposeEnergy() > 0) // EoR 5% Boost drop rate
		boostDropRate += 5;
	if (killer.getCommonData()->getCurrentSalvationPercent() > 0) // EoS 5% Boost drop rate
		boostDropRate += 5;
	if (killer.getActiveHouse() && killer.getActiveHouse()->getHouseType() == model::templates::housing::HouseType::PALACE) // Deed to Palace 5% Boost drop rate
		boostDropRate += 5;

	// Java Rates.get(killer, RatesConfig.DROP_RATES) on a snapshot of the reloadable config value
	std::shared_ptr<const std::vector<float>> dropRates = configs::main::RatesConfig::DROP_RATES.get();
	float rate = model::gameobjects::player::get(killer, dropRates ? *dropRates : std::vector<float>());
	return rate * static_cast<float>(boostDropRate) / 100.0f;
}

float DropRegistrationService::calculateEffectiveChance(const model::templates::globaldrops::GlobalRule* rule, model::gameobjects::Npc& npc, model::drop::DropModifiers& dropModifiers) {
	float chance = rule->getChance();
	// dynamic_chance means mobs will have different base chances based on their rank and rating
	if (rule->isDynamicChance())
		chance *= getRankModifier(npc) * getRatingModifier(npc);
	return dropModifiers.calculateDropChance(chance, rule->isUseLevelBasedChanceReduction());
}

int32_t DropRegistrationService::addDropItems(int32_t index, runtime::RcHashSet<runtime::Ref<model::drop::DropItem>>& droppedItems, const model::templates::globaldrops::GlobalRule* rule, model::gameobjects::Npc& npc, model::gameobjects::player::Player& player, const std::vector<runtime::Ptr<model::gameobjects::player::Player>>& groupMembers, int32_t winnerObj, model::drop::DropModifiers& dropModifiers) {
	std::vector<const GlobalDropItem*> drops = collectDrops(rule, npc, dropModifiers);
	if (!drops.empty()) {
		if (rule->getMemberLimit() > 1 && player.isInTeam()) {
			// C++: groupMembers is the caller's collection (see initDropNpc: Java null never reaches a player in a team)
			std::vector<runtime::Ptr<Player>> members(groupMembers);
			if (rule->getMemberLimit() > static_cast<int32_t>(members.size()))
				std::shuffle(members.begin(), members.end(), commons::utils::Rnd::generator()); // Java: Collections.shuffle(members)
			int32_t distributedItems = 0;
			for (const runtime::Ptr<Player>& member : members) {
				for (const GlobalDropItem* drop : drops) {
					runtime::Ref<model::drop::DropItem> dropitem = model::drop::DropItem::create(model::drop::Drop(drop->getId(), 1, 1, 100));
					dropitem->setCount(getItemCount(drop, npc));
					dropitem->setIndex(index++);
					dropitem->setPlayerObjId(member->getObjectId());
					dropitem->setWinningPlayer(member);
					dropitem->isDistributeItem(true);
					droppedItems.add(dropitem);
				}
				if (++distributedItems >= rule->getMemberLimit())
					break;
			}
		} else {
			for (const GlobalDropItem* drop : drops) {
				// Java evaluates the arguments left to right: the index first, then getItemCount's Rnd.get
				int32_t dropIndex = index++;
				int64_t count = getItemCount(drop, npc);
				droppedItems.add(regDropItem(dropIndex, winnerObj, npc.getObjectId(), drop->getId(), count));
			}
		}
	}
	return index;
}

runtime::Ref<model::drop::DropItem> DropRegistrationService::regDropItem(int32_t index, int32_t playerObjId, int32_t objId, int32_t itemId, int64_t value) {
	runtime::Ref<model::drop::DropItem> item = model::drop::DropItem::create(model::drop::Drop(itemId, 1, 1, 100));
	item->setPlayerObjId(playerObjId);
	item->setNpcObj(objId);
	item->setCount(value);
	item->setIndex(index);
	return item;
}

bool DropRegistrationService::hasGlobalNpcExclusions(model::gameobjects::Npc& npc) {
	const dataholders::GlobalNpcExclusionData& gde = *dataholders::DataManager::GLOBAL_EXCLUSION_DATA;
	if (!gde.isEmpty()) {
		if (gde.getNpcIds().contains(npc.getNpcId()) || gde.getNpcNames().contains(npc.getName())
			|| gde.getNpcTemplateTypes().contains(npc.getNpcTemplateType()) || npc.getTribe() && gde.getNpcTribes().contains(*npc.getTribe())
			|| gde.getNpcAbyssTypes().contains(npc.getAbyssNpcType()))
			return true;
	}
	return false;
}

bool DropRegistrationService::checkRuleRestrictions(const model::templates::globaldrops::GlobalRule* rule, model::Race race, model::gameobjects::Npc& npc) {
	if (!checkRestrictionRace(rule, race))
		return false;
	if (!checkGlobalRuleMaps(rule, npc))
		return false;
	if (!checkGlobalRuleWorlds(rule, npc))
		return false;
	if (!checkGlobalRuleRatings(rule, npc))
		return false;
	if (!checkGlobalRuleRaces(rule, npc))
		return false;
	if (!checkGlobalRuleTribes(rule, npc))
		return false;
	if (!checkGlobalRuleZones(rule, npc))
		return false;
	if (!checkGlobalRuleNpcs(rule, npc))
		return false;
	if (!checkGlobalRuleNpcGroups(rule, npc)) // drop group from npc_templates
		return false;
	if (!checkGlobalRuleExcludedNpcs(rule, npc))
		return false;
	return true;
}

bool DropRegistrationService::checkRestrictionRace(const model::templates::globaldrops::GlobalRule* rule, model::Race race) {
	if (rule->getRestrictionRace()) {
		if (race == model::Race::ASMODIANS && *rule->getRestrictionRace() == GlobalRule::RestrictionRace::ELYOS
			|| race == model::Race::ELYOS && *rule->getRestrictionRace() == GlobalRule::RestrictionRace::ASMODIANS)
			return false;
	}
	return true;
}

bool DropRegistrationService::checkGlobalRuleMaps(const model::templates::globaldrops::GlobalRule* rule, model::gameobjects::Npc& npc) {
	if (rule->getGlobalRuleMaps() != nullptr) {
		for (const model::templates::globaldrops::GlobalDropMap& gdMap : rule->getGlobalRuleMaps()->getGlobalDropMaps())
			if (gdMap.getMapId() == npc.getPosition()->getMapId())
				return true;
		return false;
	}
	return true;
}

bool DropRegistrationService::checkGlobalRuleWorlds(const model::templates::globaldrops::GlobalRule* rule, model::gameobjects::Npc& npc) {
	if (rule->getGlobalRuleWorlds() != nullptr) {
		for (const model::templates::globaldrops::GlobalDropWorld& gdWorld : rule->getGlobalRuleWorlds()->getGlobalDropWorlds())
			if (gdWorld.getWorldDropType() == npc.getWorldDropType())
				return true;
		return false;
	}
	return true;
}

bool DropRegistrationService::checkGlobalRuleRatings(const model::templates::globaldrops::GlobalRule* rule, model::gameobjects::Npc& npc) {
	if (rule->getGlobalRuleRatings() != nullptr) {
		for (const model::templates::globaldrops::GlobalDropRating& gdRating : rule->getGlobalRuleRatings()->getGlobalDropRatings())
			if (gdRating.getRating() == npc.getRating())
				return true;
		return false;
	}
	return true;
}

bool DropRegistrationService::checkGlobalRuleRaces(const model::templates::globaldrops::GlobalRule* rule, model::gameobjects::Npc& npc) {
	if (rule->getGlobalRuleRaces() != nullptr) {
		for (const model::templates::globaldrops::GlobalDropRace& gdRace : rule->getGlobalRuleRaces()->getGlobalDropRaces())
			if (gdRace.getRace() == npc.getRace())
				return true;
		return false;
	}
	return true;
}

bool DropRegistrationService::checkGlobalRuleTribes(const model::templates::globaldrops::GlobalRule* rule, model::gameobjects::Npc& npc) {
	if (rule->getGlobalRuleTribes() != nullptr) {
		for (const model::templates::globaldrops::GlobalDropTribe& gdTribe : rule->getGlobalRuleTribes()->getGlobalDropTribes())
			if (npc.getTribe() == gdTribe.getTribe()) // Java: gdTribe.getTribe().equals(npc.getTribe()), false for an npc without a tribe (null)
				return true;
		return false;
	}
	return true;
}

bool DropRegistrationService::checkGlobalRuleZones(const model::templates::globaldrops::GlobalRule* rule, model::gameobjects::Npc& npc) {
	if (rule->getGlobalRuleZones() != nullptr) {
		for (const model::templates::globaldrops::GlobalDropZone& gdZone : rule->getGlobalRuleZones()->getGlobalDropZones())
			if (npc.isInsideZone(world::zone::ZoneName::get(gdZone.getZone())))
				return true;
		return false;
	}
	return true;
}

bool DropRegistrationService::checkGlobalRuleNpcs(const model::templates::globaldrops::GlobalRule* rule, model::gameobjects::Npc& npc) {
	if (rule->getGlobalRuleNpcs() != nullptr) {
		for (const model::templates::globaldrops::GlobalDropNpc& gdNpc : rule->getGlobalRuleNpcs()->getGlobalDropNpcs())
			if (gdNpc.getNpcId() == npc.getNpcId())
				return true;
		return false;
	}
	return true;
}

bool DropRegistrationService::checkGlobalRuleNpcGroups(const model::templates::globaldrops::GlobalRule* rule, model::gameobjects::Npc& npc) {
	if (rule->getGlobalRuleNpcGroups() != nullptr) {
		for (const model::templates::globaldrops::GlobalDropNpcGroup& gdGroup : rule->getGlobalRuleNpcGroups()->getGlobalDropNpcGroups())
			if (gdGroup.getGroup() == npc.getGroupDrop())
				return true;
		return false;
	}
	return true;
}

bool DropRegistrationService::checkGlobalRuleExcludedNpcs(const model::templates::globaldrops::GlobalRule* rule, model::gameobjects::Npc& npc) {
	if (rule->getGlobalRuleExcludedNpcs() != nullptr) {
		const std::optional<std::unordered_set<int32_t>>& npcIds = rule->getGlobalRuleExcludedNpcs()->getNpcIds();
		if (!npcIds)
			throw runtime::NullPointerException("gd_excluded_npcs npc_ids"); // Java: getNpcIds().contains(...) on a null list
		return !npcIds->contains(npc.getNpcId());
	}
	return true;
}

std::vector<const model::templates::globaldrops::GlobalDropItem*> DropRegistrationService::collectDrops(const model::templates::globaldrops::GlobalRule* rule, model::gameobjects::Npc& npc, model::drop::DropModifiers& dropModifiers) {
	int32_t maxDrops = !dropModifiers.getMaxDropsPerGroup() ? rule->getMaxDropRule() : *dropModifiers.getMaxDropsPerGroup();
	std::vector<const GlobalDropItem*> drops = collectAllowedDrops(rule, npc, dropModifiers);
	if (static_cast<int32_t>(drops.size()) > maxDrops) {
		std::vector<const GlobalDropItem*> allowedItems;
		for (int32_t i = 0; i < maxDrops && !drops.empty(); i++) {
			const GlobalDropItem* item = model::Chance::selectElement(drops, true);
			if (item != nullptr)
				allowedItems.push_back(item);
		}
		return allowedItems;
	}
	return drops;
}

std::vector<const model::templates::globaldrops::GlobalDropItem*> DropRegistrationService::collectAllowedDrops(const model::templates::globaldrops::GlobalRule* rule, model::gameobjects::Npc& npc, model::drop::DropModifiers& dropModifiers) {
	if (!checkRuleRestrictions(rule, dropModifiers.getDropRace(), npc))
		return {};
	std::vector<const GlobalDropItem*> tempItems;
	if (!rule->getDropItems())
		throw runtime::NullPointerException("rule.getDropItems()"); // Java: a rule without gd_items has a null list
	for (const GlobalDropItem& globalItem : *rule->getDropItems()) {
		const model::templates::item::ItemTemplate* itemTemplate = dataholders::DataManager::ITEM_DATA->getItemTemplate(globalItem.getId());
		if (itemTemplate == nullptr)
			throw runtime::NullPointerException("ITEM_DATA.getItemTemplate(" + std::to_string(globalItem.getId()) + ")");
		if (itemTemplate->getRace() == model::Race::PC_ALL || itemTemplate->getRace() == dropModifiers.getDropRace()) {
			int32_t diff = npc.getLevel() - itemTemplate->getLevel();
			if (diff >= rule->getMinDiff() && diff <= rule->getMaxDiff())
				tempItems.push_back(&globalItem);
		}
	}
	return tempItems;
}

int64_t DropRegistrationService::getItemCount(const model::templates::globaldrops::GlobalDropItem* item, model::gameobjects::Npc& npc) {
	int64_t count = commons::utils::Rnd::get(item->getMinCount(), item->getMaxCount());
	if (item->getId() == model::items::ItemId::KINAH) {
		// Java: count *= npc.getLevel() * Math.pow(getRankModifier(npc) * getRatingModifier(npc), 6) - the float product widened to double, the
		// right-hand side (level * pow) evaluated first, then count * it in double and narrowed back to long (JLS 15.26.2)
		float rankRating = getRankModifier(npc) * getRatingModifier(npc);
		double factor = static_cast<double>(npc.getLevel()) * std::pow(static_cast<double>(rankRating), 6.0);
		count = javaLongCast(static_cast<double>(count) * factor);
	}
	return count;
}

float DropRegistrationService::getRankModifier(model::gameobjects::Npc& npc) {
	using model::templates::npc::NpcRank;
	switch (npc.getRank()) {
		case NpcRank::NOVICE:
			return 0.9f;
		case NpcRank::DISCIPLINED:
			return 1.0f;
		case NpcRank::SEASONED:
			return 1.05f;
		case NpcRank::EXPERT:
			return 1.1f;
		case NpcRank::VETERAN:
			return 1.15f;
		case NpcRank::MASTER:
			return 1.2f;
	}
	throw runtime::IllegalStateException("unknown NpcRank"); // Java: MatchException of an exhaustive switch
}

float DropRegistrationService::getRatingModifier(model::gameobjects::Npc& npc) {
	using model::templates::npc::NpcRating;
	switch (npc.getRating()) {
		case NpcRating::JUNK:
			return 0.5f;
		case NpcRating::NORMAL:
			return 1.0f;
		case NpcRating::ELITE:
			return 1.3f;
		case NpcRating::HERO:
			return 1.8f;
		case NpcRating::LEGENDARY:
			return 2.0f;
	}
	throw runtime::IllegalStateException("unknown NpcRating"); // Java: MatchException of an exhaustive switch
}

} // namespace aion::gameserver::services::drop
