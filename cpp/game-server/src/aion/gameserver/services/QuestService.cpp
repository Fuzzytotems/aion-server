#include "aion/gameserver/services/QuestService.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/model/templates/quest/QuestDrop.h"

namespace aion::gameserver::services {

// Anonymous classes and stored lambdas of the Java class (hub-headers.md §7.3): the bodies that create them define the structs that
// `python tools/gen/fieldmap.py --class <key>` prints.
//   anonymous Runnable at QuestService.java:815 (com.aionemu.gameserver.services.QuestService$1); argument 1 of schedule(); storage: task
//   anonymous Runnable at QuestService.java:831 (com.aionemu.gameserver.services.QuestService$2); argument 1 of schedule(); storage: task

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.services.QuestService");

runtime::HashMap<int32_t, runtime::Ref<runtime::RcArrayList<const model::templates::quest::QuestDrop*>>> QuestService::questDrop{
	AION_LOCK_CLASS(QuestService::questDrop)};

bool QuestService::finishQuest(questEngine::model::QuestEnv& env) {
	AION_UNPORTED();
}

void QuestService::validateAndFixRewardGroup(runtime::Ptr<questEngine::model::QuestState> qs, int32_t questId) {
	AION_UNPORTED();
}

std::vector<const model::templates::quest::QuestItems*> QuestService::getRewardItems(questEngine::model::QuestEnv& env,
	const model::templates::QuestTemplate* template_, bool extended, std::optional<int32_t> rewardGroup) {
	AION_UNPORTED();
}

int32_t QuestService::getRewardIndex(int32_t dialogActionId) {
	AION_UNPORTED();
}

void QuestService::giveReward(questEngine::model::QuestEnv& env, const model::templates::quest::Rewards* rewards) {
	AION_UNPORTED();
}

std::optional<commons::database::Timestamp> QuestService::calculateRepeatDate(model::gameobjects::player::Player& player,
	const model::templates::QuestTemplate* template_) {
	AION_UNPORTED();
}

model::templates::quest::QuestRepeatCycle QuestService::findNextRepeatDay(
	const std::vector<model::templates::quest::QuestRepeatCycle>& questRepeatDays, std::chrono::weekday day) {
	AION_UNPORTED();
}

bool QuestService::checkStartConditions(model::gameobjects::player::Player& player, int32_t questId, bool warn) {
	AION_UNPORTED();
}

bool QuestService::checkStartConditions(model::gameobjects::player::Player& player, int32_t questId, bool warn, int32_t allowedDiffToMinLevel,
	bool skipStartedCheck, bool skipRepeatCountCheck, bool skipXmlPreconditionCheck) {
	AION_UNPORTED();
}

bool QuestService::startQuest(questEngine::model::QuestEnv& env) {
	AION_UNPORTED();
}

bool QuestService::startQuest(questEngine::model::QuestEnv& env, questEngine::model::QuestStatus status, bool warn) {
	AION_UNPORTED();
}

void QuestService::addOrUpdateQuest(model::gameobjects::player::Player& player, int32_t questId, questEngine::model::QuestStatus status) {
	AION_UNPORTED();
}

bool QuestService::checkCombineSkill(questEngine::model::QuestEnv& env, bool warn) {
	AION_UNPORTED();
}

bool QuestService::startEventQuest(questEngine::model::QuestEnv& env, questEngine::model::QuestStatus questStatus) {
	AION_UNPORTED();
}

bool QuestService::checkQuestListSize(model::gameobjects::player::QuestStateList& qsl) {
	AION_UNPORTED();
}

bool QuestService::collectItemCheck(questEngine::model::QuestEnv& env, bool removeItem) {
	AION_UNPORTED();
}

bool QuestService::inventoryItemCheck(questEngine::model::QuestEnv& env, bool showWarning) {
	AION_UNPORTED();
}

int32_t QuestService::checkAndGetCollectItemQuestRewardCategory(questEngine::model::QuestEnv& env) {
	AION_UNPORTED();
}

int32_t QuestService::checkAndGetCollectItemQuestRewardCategory(questEngine::model::QuestEnv& env, std::optional<int32_t> rewardIndex) {
	AION_UNPORTED();
}

int32_t QuestService::getQuestDrop(const std::unordered_set<runtime::Ptr<model::drop::DropItem>>& dropItems, int32_t index,
	model::gameobjects::Npc& npc, const std::vector<runtime::Ptr<model::gameobjects::player::Player>>& players,
	model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

void QuestService::allowLooting(const std::vector<runtime::Ptr<model::gameobjects::player::Player>>& players, model::gameobjects::DropNpc& dropNpc,
	runtime::Ptr<model::drop::DropItem> dropItem) {
	AION_UNPORTED();
}

runtime::Ref<model::drop::DropItem> QuestService::regQuestDropItem(const model::templates::quest::QuestDrop* drop, int32_t index,
	std::optional<int32_t> winner) {
	AION_UNPORTED();
}

bool QuestService::isQuestDrop(model::gameobjects::player::Player& player, const model::templates::quest::QuestDrop* drop) {
	AION_UNPORTED();
}

bool QuestService::checkLevelRequirement(int32_t questId, int32_t playerLevel) {
	AION_UNPORTED();
}

bool QuestService::checkLevelRequirement(const model::templates::QuestTemplate* qt, int32_t playerLevel) {
	AION_UNPORTED();
}

int32_t QuestService::getLevelRequirementDiff(int32_t questId, int32_t playerLevel) {
	AION_UNPORTED();
}

bool QuestService::questTimerStart(questEngine::model::QuestEnv& env, int32_t timeInSeconds) {
	AION_UNPORTED();
}

bool QuestService::invisibleTimerStart(questEngine::model::QuestEnv& env, int32_t timeInSeconds) {
	AION_UNPORTED();
}

bool QuestService::questTimerEnd(questEngine::model::QuestEnv& env) {
	AION_UNPORTED();
}

bool QuestService::abandonQuest(model::gameobjects::player::Player& player, int32_t questId) {
	AION_UNPORTED();
}

std::vector<const model::templates::quest::QuestDrop*> QuestService::getQuestDrop(int32_t npcId) {
	AION_UNPORTED();
}

void QuestService::addQuestDrop(int32_t npcId, const model::templates::quest::QuestDrop* drop) {
	AION_UNPORTED();
}

void QuestService::clearQuestDrops() {
	AION_UNPORTED();
}

std::vector<runtime::Ptr<model::gameobjects::player::Player>> QuestService::getEachDropMembersGroup(model::team::group::PlayerGroup& group,
	int32_t npcId, int32_t questId) {
	AION_UNPORTED();
}

std::vector<runtime::Ptr<model::gameobjects::player::Player>> QuestService::getEachDropMembersAlliance(
	model::team::alliance::PlayerAlliance& alliance, int32_t npcId, int32_t questId) {
	AION_UNPORTED();
}

void QuestService::removeQuestWorkItems(model::gameobjects::player::Player& player, questEngine::model::QuestState& qs) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::services
