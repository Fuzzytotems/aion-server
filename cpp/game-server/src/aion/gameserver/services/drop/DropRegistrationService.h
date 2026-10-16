#pragma once

#include <cstdint>
#include <optional>
#include <unordered_set>
#include <vector>

#include "aion/gameserver/runtime/collections/ConcurrentHashMap.h"
#include "aion/gameserver/runtime/collections/Rc.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/model/drop/fwd.h"
#include "aion/gameserver/model/fwd.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/templates/globaldrops/fwd.h"
#include "aion/gameserver/services/drop/fwd.h"

namespace aion::gameserver::services::drop {

/**
 * C++: an Immortal singleton (hub-headers.md §11.2) with a private constructor and destructor; getInstance() is Java's SingletonHolder.
 * droppedItems is the live set that registerDrop stores in currentDropMap and the callees fill (RcHashSet&); allowedLooters is the caller's
 * list that initDropNpc fills.
 *
 * @author xTz, Aioncool, Bobobear, Neon
 */
class DropRegistrationService : public runtime::Immortal {
private:
	runtime::ConcurrentHashMap<int32_t, runtime::Ref<runtime::RcHashSet<runtime::Ref<model::drop::DropItem>>>> currentDropMap{AION_LOCK_CLASS(DropRegistrationService::currentDropMap#stripe)}; // Java: = new ConcurrentHashMap<>()
	runtime::ConcurrentHashMap<int32_t, runtime::Ref<model::gameobjects::DropNpc>> dropRegistrationMap{AION_LOCK_CLASS(DropRegistrationService::dropRegistrationMap#stripe)}; // Java: = new ConcurrentHashMap<>()
	DropRegistrationService();
	~DropRegistrationService();
public:
	void registerDrop(model::gameobjects::Npc& npc, model::gameobjects::player::Player& player, const std::vector<runtime::Ptr<model::gameobjects::player::Player>>& groupMembers);
	/** After NPC dies, it can register arbitrary drop */
	void registerDrop(model::gameobjects::Npc& npc, model::gameobjects::player::Player& player, int32_t highestLevel, const std::vector<runtime::Ptr<model::gameobjects::player::Player>>& groupMembers);
	model::drop::DropModifiers createDropModifiers(model::gameobjects::Npc& npc, model::gameobjects::player::Player& player, int32_t highestLevel);
private:
	runtime::Ptr<model::gameobjects::player::Player> initDropNpc(model::gameobjects::player::Player& player, int32_t npcObjId, std::vector<runtime::Ptr<model::gameobjects::player::Player>>& allowedLooters, const std::vector<runtime::Ptr<model::gameobjects::player::Player>>& groupMembers);
public:
	bool isAllowedDefaultGlobalDropNpc(model::gameobjects::Npc& npc, bool isChest);
private:
	int32_t addGlobalDrops(int32_t index, model::drop::DropModifiers& dropModifiers, model::gameobjects::player::Player& player, model::gameobjects::Npc& npc, bool isAllowedDefaultGlobalDropNpc, const std::vector<const model::templates::globaldrops::GlobalRule*>& rules, runtime::RcHashSet<runtime::Ref<model::drop::DropItem>>& droppedItems, const std::vector<runtime::Ptr<model::gameobjects::player::Player>>& groupMembers, int32_t winnerObj);
	std::optional<float> getReductionDropRate(model::gameobjects::Npc& npc, int32_t highestLevel);
	float calculateBoostDropRate(model::gameobjects::player::Player& killer, model::gameobjects::Npc& npc);
public:
	float calculateEffectiveChance(const model::templates::globaldrops::GlobalRule* rule, model::gameobjects::Npc& npc, model::drop::DropModifiers& dropModifiers);
private:
	int32_t addDropItems(int32_t index, runtime::RcHashSet<runtime::Ref<model::drop::DropItem>>& droppedItems, const model::templates::globaldrops::GlobalRule* rule, model::gameobjects::Npc& npc, model::gameobjects::player::Player& player, const std::vector<runtime::Ptr<model::gameobjects::player::Player>>& groupMembers, int32_t winnerObj, model::drop::DropModifiers& dropModifiers);
public:
	runtime::Ref<model::drop::DropItem> regDropItem(int32_t index, int32_t playerObjId, int32_t objId, int32_t itemId, int64_t count);
	runtime::ConcurrentHashMap<int32_t, runtime::Ref<model::gameobjects::DropNpc>>& getDropRegistrationMap() { return this->dropRegistrationMap; }
	runtime::ConcurrentHashMap<int32_t, runtime::Ref<runtime::RcHashSet<runtime::Ref<model::drop::DropItem>>>>& getCurrentDropMap() { return this->currentDropMap; }
	static DropRegistrationService& getInstance(); // Java singleton
	bool hasGlobalNpcExclusions(model::gameobjects::Npc& npc);
private:
	bool checkRuleRestrictions(const model::templates::globaldrops::GlobalRule* rule, model::Race race, model::gameobjects::Npc& npc);
	bool checkRestrictionRace(const model::templates::globaldrops::GlobalRule* rule, model::Race race);
	bool checkGlobalRuleMaps(const model::templates::globaldrops::GlobalRule* rule, model::gameobjects::Npc& npc);
	bool checkGlobalRuleWorlds(const model::templates::globaldrops::GlobalRule* rule, model::gameobjects::Npc& npc);
	bool checkGlobalRuleRatings(const model::templates::globaldrops::GlobalRule* rule, model::gameobjects::Npc& npc);
	bool checkGlobalRuleRaces(const model::templates::globaldrops::GlobalRule* rule, model::gameobjects::Npc& npc);
	bool checkGlobalRuleTribes(const model::templates::globaldrops::GlobalRule* rule, model::gameobjects::Npc& npc);
	bool checkGlobalRuleZones(const model::templates::globaldrops::GlobalRule* rule, model::gameobjects::Npc& npc);
	bool checkGlobalRuleNpcs(const model::templates::globaldrops::GlobalRule* rule, model::gameobjects::Npc& npc);
	bool checkGlobalRuleNpcGroups(const model::templates::globaldrops::GlobalRule* rule, model::gameobjects::Npc& npc);
	bool checkGlobalRuleExcludedNpcs(const model::templates::globaldrops::GlobalRule* rule, model::gameobjects::Npc& npc);
public:
	std::vector<const model::templates::globaldrops::GlobalDropItem*> collectDrops(const model::templates::globaldrops::GlobalRule* rule, model::gameobjects::Npc& npc, model::drop::DropModifiers& dropModifiers);
private:
	std::vector<const model::templates::globaldrops::GlobalDropItem*> collectAllowedDrops(const model::templates::globaldrops::GlobalRule* rule, model::gameobjects::Npc& npc, model::drop::DropModifiers& dropModifiers);
	int64_t getItemCount(const model::templates::globaldrops::GlobalDropItem* item, model::gameobjects::Npc& npc);
	float getRankModifier(model::gameobjects::Npc& npc);
	float getRatingModifier(model::gameobjects::Npc& npc);
};

} // namespace aion::gameserver::services::drop
