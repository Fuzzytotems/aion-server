#pragma once

#include <chrono>
#include <cstdint>
#include <optional>
#include <unordered_set>
#include <vector>

#include "aion/gameserver/runtime/collections/HashMap.h"
#include "aion/gameserver/runtime/collections/Rc.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/commons/database/SqlTypes.h"
#include "aion/gameserver/model/drop/fwd.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/team/alliance/fwd.h"
#include "aion/gameserver/model/team/group/fwd.h"
#include "aion/gameserver/model/templates/fwd.h"
#include "aion/gameserver/model/templates/quest/fwd.h"
#include "aion/gameserver/questEngine/model/fwd.h"
#include "aion/gameserver/services/fwd.h"

namespace aion::gameserver::services {

/**
 * @author Mr. Poke, vlog, bobobear, xTz, Rolandas
 */
class QuestService final {
private:
	/** C++: defined in QuestService.cpp (hub-headers.md §3.1: no full template header here) */
	static runtime::HashMap<int32_t, runtime::Ref<runtime::RcArrayList<const model::templates::quest::QuestDrop*>>> questDrop;
public:
	/** Finishes the quest and rewards the player. */
	static bool finishQuest(questEngine::model::QuestEnv& env);
	/**
	 * Validates and sets/corrects (if necessary) the reward group which is to be used. Must only be called in reward state.
	 */
	static void validateAndFixRewardGroup(runtime::Ptr<questEngine::model::QuestState> qs, int32_t questId);
private:
	static std::vector<const model::templates::quest::QuestItems*> getRewardItems(questEngine::model::QuestEnv& env,
		const model::templates::QuestTemplate* template_, bool extended, std::optional<int32_t> rewardGroup);
public:
	/** Converts the dialog action ID to the corresponding reward ID. */
	static int32_t getRewardIndex(int32_t dialogActionId);
private:
	static void giveReward(questEngine::model::QuestEnv& env, const model::templates::quest::Rewards* rewards);
	static std::optional<commons::database::Timestamp> calculateRepeatDate(model::gameobjects::player::Player& player,
		const model::templates::QuestTemplate* template_);
	/** C++: java.time.DayOfWeek is std::chrono::weekday (Java getValue() is iso_encoding()) */
	static model::templates::quest::QuestRepeatCycle findNextRepeatDay(const std::vector<model::templates::quest::QuestRepeatCycle>& questRepeatDays,
		std::chrono::weekday day);
public:
	static bool checkStartConditions(model::gameobjects::player::Player& player, int32_t questId, bool warn);
	/**
	 * Checks if the player meets all required conditions to start the specified quest.<br>
	 * This method will not propagate any exceptions to the caller
	 */
	static bool checkStartConditions(model::gameobjects::player::Player& player, int32_t questId, bool warn, int32_t allowedDiffToMinLevel,
		bool skipStartedCheck, bool skipRepeatCountCheck, bool skipXmlPreconditionCheck);
	static bool startQuest(questEngine::model::QuestEnv& env);
	static bool startQuest(questEngine::model::QuestEnv& env, questEngine::model::QuestStatus status, bool warn);
	/** Adds the quest to the players quest list. */
	static void addOrUpdateQuest(model::gameobjects::player::Player& player, int32_t questId, questEngine::model::QuestStatus status);
	/** Checks if the crafting/tapping skill point requirements for this quest */
	static bool checkCombineSkill(questEngine::model::QuestEnv& env, bool warn);
	static bool startEventQuest(questEngine::model::QuestEnv& env, questEngine::model::QuestStatus questStatus);
private:
	static bool checkQuestListSize(model::gameobjects::player::QuestStateList& qsl);
public:
	static bool collectItemCheck(questEngine::model::QuestEnv& env, bool removeItem);
	static bool inventoryItemCheck(questEngine::model::QuestEnv& env, bool showWarning);
	/**
	 * Only used by relic reward quests. Checks if the player has any necessary items with sufficient count and starts the quest.
	 */
	static int32_t checkAndGetCollectItemQuestRewardCategory(questEngine::model::QuestEnv& env);
	static int32_t checkAndGetCollectItemQuestRewardCategory(questEngine::model::QuestEnv& env, std::optional<int32_t> rewardIndex);
	static int32_t getQuestDrop(const std::unordered_set<runtime::Ptr<model::drop::DropItem>>& dropItems, int32_t index, model::gameobjects::Npc& npc,
		const std::vector<runtime::Ptr<model::gameobjects::player::Player>>& players, model::gameobjects::player::Player& player);
private:
	static void allowLooting(const std::vector<runtime::Ptr<model::gameobjects::player::Player>>& players, model::gameobjects::DropNpc& dropNpc,
		runtime::Ptr<model::drop::DropItem> dropItem);
	static runtime::Ref<model::drop::DropItem> regQuestDropItem(const model::templates::quest::QuestDrop* drop, int32_t index,
		std::optional<int32_t> winner);
	static bool isQuestDrop(model::gameobjects::player::Player& player, const model::templates::quest::QuestDrop* drop);
public:
	static bool checkLevelRequirement(int32_t questId, int32_t playerLevel);
	static bool checkLevelRequirement(const model::templates::QuestTemplate* qt, int32_t playerLevel);
	static int32_t getLevelRequirementDiff(int32_t questId, int32_t playerLevel);
	static bool questTimerStart(questEngine::model::QuestEnv& env, int32_t timeInSeconds);
	static bool invisibleTimerStart(questEngine::model::QuestEnv& env, int32_t timeInSeconds);
	static bool questTimerEnd(questEngine::model::QuestEnv& env);
	static bool abandonQuest(model::gameobjects::player::Player& player, int32_t questId);
	static std::vector<const model::templates::quest::QuestDrop*> getQuestDrop(int32_t npcId);
	static void addQuestDrop(int32_t npcId, const model::templates::quest::QuestDrop* drop);
	/** Clears all quest drop info (used when reloading quest data) */
	static void clearQuestDrops();
	static std::vector<runtime::Ptr<model::gameobjects::player::Player>> getEachDropMembersGroup(model::team::group::PlayerGroup& group, int32_t npcId,
		int32_t questId);
	static std::vector<runtime::Ptr<model::gameobjects::player::Player>> getEachDropMembersAlliance(model::team::alliance::PlayerAlliance& alliance,
		int32_t npcId, int32_t questId);
	static void removeQuestWorkItems(model::gameobjects::player::Player& player, questEngine::model::QuestState& qs);
};

} // namespace aion::gameserver::services
