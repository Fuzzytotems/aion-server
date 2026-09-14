#include "aion/gameserver/questEngine/QuestEngine.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/questEngine/handlers/AbstractQuestHandler.h"
#include "aion/gameserver/services/cron/CronService.h"

// Member types (docs/design/hub-headers.md §3.3): the constructor, the destructor and getInstance() instantiate the destructors of the
// Ref<QuestNpc> map (QuestNpc: model/templates/quest, not a hub; S0c declaration header, §3.5). Remove the guard in the change that adds it.
#if __has_include("aion/gameserver/model/templates/quest/QuestNpc.h")
#define AION_S0B_QUEST_ENGINE_MEMBERS 1
#include "aion/gameserver/model/templates/quest/QuestNpc.h"
#else
#define AION_S0B_QUEST_ENGINE_MEMBERS 0
#endif

namespace aion::gameserver::questEngine {

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.questEngine.QuestEngine");

#if AION_S0B_QUEST_ENGINE_MEMBERS
QuestEngine::QuestEngine() = default;

QuestEngine::~QuestEngine() = default;

QuestEngine& QuestEngine::getInstance() {
	static QuestEngine instance; // Java SingletonHolder
	return instance;
}
#endif

// lambda at QuestEngine.java:108 (fieldmap key QuestEngine@L108:55): executeLongRunning task running QuestSpawnAnalyzer, pin {this}
void QuestEngine::init() {
	AION_UNPORTED();
}

void QuestEngine::reload() {
	AION_UNPORTED();
}

void QuestEngine::clear() {
	AION_UNPORTED();
}

bool QuestEngine::onDialog(model::QuestEnv& env) {
	AION_UNPORTED();
}

bool QuestEngine::onKill(model::QuestEnv& env) {
	AION_UNPORTED();
}

bool QuestEngine::onAttack(model::QuestEnv& env) {
	AION_UNPORTED();
}

void QuestEngine::sendCompletedQuests(gameserver::model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

void QuestEngine::onLevelChanged(gameserver::model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

void QuestEngine::onQuestCompleted(gameserver::model::gameobjects::player::Player& player, int32_t questId) {
	AION_UNPORTED();
}

void QuestEngine::onDie(model::QuestEnv& env) {
	AION_UNPORTED();
}

void QuestEngine::onLogOut(model::QuestEnv& env) {
	AION_UNPORTED();
}

void QuestEngine::onNpcReachTarget(model::QuestEnv& env) {
	AION_UNPORTED();
}

void QuestEngine::onNpcLostTarget(model::QuestEnv& env) {
	AION_UNPORTED();
}

void QuestEngine::onPassFlyingRing(model::QuestEnv& env, std::string_view flyRing) {
	AION_UNPORTED();
}

void QuestEngine::onEnterWorld(gameserver::model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

handlers::HandlerResult QuestEngine::onItemUseEvent(model::QuestEnv& env, gameserver::model::gameobjects::Item& item) {
	AION_UNPORTED();
}

void QuestEngine::onHouseItemUseEvent(model::QuestEnv& env) {
	AION_UNPORTED();
}

void QuestEngine::onItemGet(gameserver::model::gameobjects::player::Player& player, int32_t itemId) {
	AION_UNPORTED();
}

void QuestEngine::onItemRemoved(gameserver::model::gameobjects::player::Player& player, int32_t itemId) {
	AION_UNPORTED();
}

bool QuestEngine::onKillRanked(model::QuestEnv& env, utils::stats::AbyssRankEnum playerRank) {
	AION_UNPORTED();
}

bool QuestEngine::onKillInWorld(model::QuestEnv& env, int32_t worldId) {
	AION_UNPORTED();
}

bool QuestEngine::onKillInZone(model::QuestEnv& env, std::string_view zoneName) {
	AION_UNPORTED();
}

bool QuestEngine::onEnterZone(model::QuestEnv& env, const world::zone::ZoneName* zoneName) {
	AION_UNPORTED();
}

bool QuestEngine::onLeaveZone(model::QuestEnv& env, const world::zone::ZoneName* zoneName) {
	AION_UNPORTED();
}

void QuestEngine::onMovieEnd(model::QuestEnv& env, int32_t movieId) {
	AION_UNPORTED();
}

void QuestEngine::onQuestTimerEnd(model::QuestEnv& env) {
	AION_UNPORTED();
}

void QuestEngine::onInvisibleTimerEnd(model::QuestEnv& env) {
	AION_UNPORTED();
}

bool QuestEngine::onUseSkill(model::QuestEnv& env, int32_t skillId) {
	AION_UNPORTED();
}

void QuestEngine::onFailCraft(model::QuestEnv& env, int32_t itemId) {
	AION_UNPORTED();
}

void QuestEngine::onEquipItem(model::QuestEnv& env, int32_t itemId) {
	AION_UNPORTED();
}

bool QuestEngine::onCanAct(model::QuestEnv& env, int32_t templateId, model::QuestActionType questActionType,
	std::initializer_list<std::any> objects) {
	AION_UNPORTED();
}

void QuestEngine::onDredgionReward(model::QuestEnv& env) {
	AION_UNPORTED();
}

handlers::HandlerResult QuestEngine::onBonusApplyEvent(model::QuestEnv& env, gameserver::model::templates::rewards::BonusType bonusType,
	const std::vector<const gameserver::model::templates::quest::QuestItems*>& rewardItems) {
	AION_UNPORTED();
}

bool QuestEngine::onAddAggroList(model::QuestEnv& env) {
	AION_UNPORTED();
}

bool QuestEngine::onAtDistance(model::QuestEnv& env) {
	AION_UNPORTED();
}

void QuestEngine::onEnterWindStream(model::QuestEnv& env, int32_t loc) {
	AION_UNPORTED();
}

void QuestEngine::rideAction(model::QuestEnv& env, int32_t itemId) {
	AION_UNPORTED();
}

runtime::Ptr<gameserver::model::templates::quest::QuestNpc> QuestEngine::registerQuestNpc(int32_t npcId) {
	AION_UNPORTED();
}

runtime::Ptr<gameserver::model::templates::quest::QuestNpc> QuestEngine::registerQuestNpc(int32_t npcId, int32_t range) {
	AION_UNPORTED();
}

void QuestEngine::registerQuestItem(int32_t itemId, int32_t questId) {
	AION_UNPORTED();
}

bool QuestEngine::isRegisteredQuestItem(int32_t itemId) {
	AION_UNPORTED();
}

void QuestEngine::registerQuestHouseItem(int32_t questId) {
	AION_UNPORTED();
}

void QuestEngine::registerOnGetItem(int32_t itemId, int32_t questId) {
	AION_UNPORTED();
}

void QuestEngine::registerOnLevelChanged(int32_t questId) {
	AION_UNPORTED();
}

runtime::Ptr<runtime::RcArrayList<int32_t>> QuestEngine::getOrCreateOnLevelUpForRace(gameserver::model::Race race) {
	AION_UNPORTED();
}

void QuestEngine::registerOnQuestCompleted(int32_t questId) {
	AION_UNPORTED();
}

void QuestEngine::registerOnEnterWorld(int32_t questId) {
	AION_UNPORTED();
}

void QuestEngine::registerOnDie(int32_t questId) {
	AION_UNPORTED();
}

void QuestEngine::registerOnLogOut(int32_t questId) {
	AION_UNPORTED();
}

void QuestEngine::registerOnEnterZone(const world::zone::ZoneName* zoneName, int32_t questId) {
	AION_UNPORTED();
}

void QuestEngine::registerOnKillInZone(std::string_view zone, int32_t questId) {
	AION_UNPORTED();
}

void QuestEngine::registerOnLeaveZone(const world::zone::ZoneName* zoneName, int32_t questId) {
	AION_UNPORTED();
}

void QuestEngine::registerOnKillRanked(utils::stats::AbyssRankEnum playerRank, int32_t questId) {
	AION_UNPORTED();
}

void QuestEngine::registerOnKillInWorld(int32_t worldId, int32_t questId) {
	AION_UNPORTED();
}

void QuestEngine::registerOnPassFlyingRings(std::string_view flyingRing, int32_t questId) {
	AION_UNPORTED();
}

void QuestEngine::registerOnQuestTimerEnd(int32_t questId) {
	AION_UNPORTED();
}

void QuestEngine::registerOnInvisibleTimerEnd(int32_t questId) {
	AION_UNPORTED();
}

void QuestEngine::registerQuestSkill(int32_t skillId, int32_t questId) {
	AION_UNPORTED();
}

void QuestEngine::registerOnFailCraft(int32_t itemId, int32_t questId) {
	AION_UNPORTED();
}

void QuestEngine::registerOnEquipItem(int32_t itemId, int32_t questId) {
	AION_UNPORTED();
}

bool QuestEngine::registerCanAct(int32_t questId, int32_t npcId) {
	AION_UNPORTED();
}

void QuestEngine::registerOnDredgionReward(int32_t questId) {
	AION_UNPORTED();
}

void QuestEngine::registerOnBonusApply(int32_t questId, gameserver::model::templates::rewards::BonusType bonusType) {
	AION_UNPORTED();
}

void QuestEngine::registerAddOnReachTargetEvent(int32_t questId) {
	AION_UNPORTED();
}

void QuestEngine::registerAddOnLostTargetEvent(int32_t questId) {
	AION_UNPORTED();
}

void QuestEngine::registerOnEnterWindStream(int32_t questId) {
	AION_UNPORTED();
}

void QuestEngine::registerOnRide(int32_t questId) {
	AION_UNPORTED();
}

runtime::Ref<gameserver::model::templates::quest::QuestNpc> QuestEngine::getQuestNpc(int32_t npcId) {
	AION_UNPORTED();
}

handlers::AbstractQuestHandler* QuestEngine::getQuestHandlerByQuestId(int32_t questId) {
	AION_UNPORTED();
}

int32_t QuestEngine::getQuestHandlerCount() {
	AION_UNPORTED();
}

bool QuestEngine::isHaveHandler(int32_t questId) {
	AION_UNPORTED();
}

void QuestEngine::addQuestHandler(std::unique_ptr<handlers::AbstractQuestHandler> questHandler) {
	AION_UNPORTED();
}

void QuestEngine::addHandlerSideQuestDrop(int32_t questId, int32_t npcId, int32_t itemId, int32_t amount, int32_t chance) {
	AION_UNPORTED();
}

void QuestEngine::addHandlerSideQuestDrop(int32_t questId, int32_t npcId, int32_t itemId, int32_t amount, int32_t chance, int32_t step) {
	AION_UNPORTED();
}

// lambda at QuestEngine.java:912 (com.aionemu.gameserver.questEngine.QuestEngine@L912:52): CronService job, captureless
void QuestEngine::addMessageSendingTask() {
	AION_UNPORTED();
}

} // namespace aion::gameserver::questEngine
