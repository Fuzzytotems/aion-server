#include "aion/gameserver/questEngine/handlers/AbstractQuestHandler.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/questEngine/QuestEngine.h"

namespace aion::gameserver::questEngine::handlers {

AbstractQuestHandler::AbstractQuestHandler(int32_t questIdValue) : qe(QuestEngine::getInstance()), questId(questIdValue) {
	// Java: template = DataManager.QUEST_DATA.getQuestById(questId); if (template != null) { loadWorkItems(template); loadActionItems(template); }
	AION_UNPORTED();
}

AbstractQuestHandler::~AbstractQuestHandler() = default;

void AbstractQuestHandler::loadWorkItems(const gameserver::model::templates::QuestTemplate* template_) {
	AION_UNPORTED();
}

void AbstractQuestHandler::loadActionItems(const gameserver::model::templates::QuestTemplate* template_) {
	AION_UNPORTED();
}

std::unordered_set<int32_t> AbstractQuestHandler::getActionItems() {
	AION_UNPORTED();
}

bool AbstractQuestHandler::onDialogEvent(model::QuestEnv& env) {
	AION_UNPORTED();
}

bool AbstractQuestHandler::onCanAct(model::QuestEnv& env, model::QuestActionType questEventType, std::span<const std::any> objects) {
	AION_UNPORTED();
}

void AbstractQuestHandler::updateQuestStatus(model::QuestEnv& env) {
	AION_UNPORTED();
}

bool AbstractQuestHandler::changeQuestStep(model::QuestEnv& env, int32_t oldStep, int32_t newStep) {
	AION_UNPORTED();
}

bool AbstractQuestHandler::changeQuestStep(model::QuestEnv& env, int32_t step, int32_t nextStep, bool reward) {
	AION_UNPORTED();
}

bool AbstractQuestHandler::changeQuestStep(model::QuestEnv& env, int32_t step, int32_t nextStep, bool reward, int32_t varNum) {
	AION_UNPORTED();
}

bool AbstractQuestHandler::sendQuestDialog(model::QuestEnv& env, int32_t dialogPageId) {
	AION_UNPORTED();
}

void AbstractQuestHandler::sendDialogPacket(model::QuestEnv& env, int32_t dialogPageId, int32_t value) {
	AION_UNPORTED();
}

bool AbstractQuestHandler::sendQuestSelectionDialog(model::QuestEnv& env) {
	AION_UNPORTED();
}

bool AbstractQuestHandler::closeDialogWindow(model::QuestEnv& env) {
	AION_UNPORTED();
}

bool AbstractQuestHandler::sendQuestStartDialog(model::QuestEnv& env) {
	AION_UNPORTED();
}

bool AbstractQuestHandler::sendQuestStartDialog(model::QuestEnv& env, const gameserver::model::templates::quest::QuestItems* workItem) {
	AION_UNPORTED();
}

bool AbstractQuestHandler::sendQuestStartDialog(model::QuestEnv& env, int32_t itemId, int64_t itemCount) {
	AION_UNPORTED();
}

bool AbstractQuestHandler::sendQuestEndDialog(model::QuestEnv& env, std::span<const int32_t> questItemsToRemove) {
	AION_UNPORTED();
}

bool AbstractQuestHandler::sendQuestEndDialog(model::QuestEnv& env) {
	AION_UNPORTED();
}

bool AbstractQuestHandler::isAcceptableQuest(const gameserver::model::templates::QuestTemplate* quest) {
	AION_UNPORTED();
}

bool AbstractQuestHandler::defaultCloseDialog(model::QuestEnv& env, int32_t step, int32_t nextStep) {
	AION_UNPORTED();
}

bool AbstractQuestHandler::defaultCloseDialog(model::QuestEnv& env, int32_t step, int32_t nextStep, int32_t giveItemId, int64_t giveItemCount) {
	AION_UNPORTED();
}

bool AbstractQuestHandler::defaultCloseDialog(model::QuestEnv& env, int32_t step, int32_t nextStep, bool reward, bool sameNpc) {
	AION_UNPORTED();
}

bool AbstractQuestHandler::defaultCloseDialog(model::QuestEnv& env, int32_t step, int32_t nextStep, bool reward, bool sameNpc,
	const gameserver::model::templates::quest::QuestItems* questItemToAdd) {
	AION_UNPORTED();
}

bool AbstractQuestHandler::defaultCloseDialog(model::QuestEnv& env, int32_t step, int32_t nextStep, int32_t giveItemId, int64_t giveItemCount,
	int32_t removeItemId, int64_t removeItemCount) {
	AION_UNPORTED();
}

bool AbstractQuestHandler::defaultCloseDialog(model::QuestEnv& env, int32_t step, int32_t nextStep, bool reward, bool sameNpc, int32_t giveItemId,
	int64_t giveItemCount, int32_t removeItemId, int64_t removeItemCount) {
	AION_UNPORTED();
}

bool AbstractQuestHandler::checkQuestItems(model::QuestEnv& env, int32_t step, int32_t nextStep, bool reward, int32_t checkOkId,
	int32_t checkFailId) {
	AION_UNPORTED();
}

bool AbstractQuestHandler::checkQuestItems(model::QuestEnv& env, int32_t step, int32_t nextStep, bool reward, int32_t checkOkId, int32_t checkFailId,
	int32_t giveItemId, int32_t giveItemCount) {
	AION_UNPORTED();
}

bool AbstractQuestHandler::checkQuestItemsSimple(model::QuestEnv& env, int32_t step, int32_t nextStep, bool reward, int32_t checkOkId,
	int32_t giveItemId, int32_t giveItemCount) {
	AION_UNPORTED();
}

bool AbstractQuestHandler::checkItemExistence(model::QuestEnv& env, int32_t step, int32_t nextStep, bool reward, int32_t itemId, int32_t itemCount,
	bool remove, int32_t checkOkId, int32_t checkFailId, int32_t giveItemId, int32_t giveItemCount) {
	AION_UNPORTED();
}

bool AbstractQuestHandler::checkItemExistence(model::QuestEnv& env, int32_t itemId, int32_t itemCount, bool remove) {
	AION_UNPORTED();
}

void AbstractQuestHandler::sendEmotion(model::QuestEnv& env, gameserver::model::gameobjects::Creature& emoteCreature,
	gameserver::model::EmotionId emotion, bool broadcast) {
	AION_UNPORTED();
}

bool AbstractQuestHandler::giveQuestItem(model::QuestEnv& env, int32_t itemId, int64_t itemCount) {
	AION_UNPORTED();
}

bool AbstractQuestHandler::giveQuestItem(model::QuestEnv& env, int32_t itemId, int64_t itemCount,
	services::item::ItemPacketService_ItemAddType addType) {
	AION_UNPORTED();
}

bool AbstractQuestHandler::giveQuestItem(model::QuestEnv& env, int32_t itemId, int64_t itemCount,
	services::item::ItemPacketService_ItemAddType addType, services::item::ItemPacketService_ItemUpdateType updateType) {
	AION_UNPORTED();
}

bool AbstractQuestHandler::removeQuestItem(model::QuestEnv& env, int32_t itemId, int64_t itemCount) {
	AION_UNPORTED();
}

bool AbstractQuestHandler::removeQuestItem(model::QuestEnv& env, int32_t itemId, int64_t itemCount, model::QuestStatus questStatus) {
	AION_UNPORTED();
}

bool AbstractQuestHandler::playQuestMovie(model::QuestEnv& env, int32_t movieId) {
	AION_UNPORTED();
}

bool AbstractQuestHandler::playQuestMovie(model::QuestEnv& env, int32_t cutsceneId, bool isCutsceneMovie) {
	AION_UNPORTED();
}

bool AbstractQuestHandler::defaultOnKillEvent(model::QuestEnv& env, int32_t npcId, int32_t startVar, int32_t endVar) {
	AION_UNPORTED();
}

bool AbstractQuestHandler::defaultOnKillEvent(model::QuestEnv& env, std::span<const int32_t> npcIds, int32_t startVar, int32_t endVar) {
	AION_UNPORTED();
}

bool AbstractQuestHandler::defaultOnKillEvent(model::QuestEnv& env, int32_t npcId, int32_t startVar, int32_t endVar, int32_t varNum) {
	AION_UNPORTED();
}

bool AbstractQuestHandler::defaultOnKillEvent(model::QuestEnv& env, std::span<const int32_t> npcIds, int32_t startVar, int32_t endVar,
	int32_t varNum) {
	AION_UNPORTED();
}

bool AbstractQuestHandler::defaultOnKillEvent(model::QuestEnv& env, int32_t npcId, int32_t startVar, bool reward) {
	AION_UNPORTED();
}

bool AbstractQuestHandler::defaultOnKillEvent(model::QuestEnv& env, int32_t npcId, int32_t startVar, bool reward, int32_t varNum) {
	AION_UNPORTED();
}

bool AbstractQuestHandler::defaultOnKillEvent(model::QuestEnv& env, std::span<const int32_t> npcIds, int32_t startVar, bool reward) {
	AION_UNPORTED();
}

bool AbstractQuestHandler::defaultOnKillEvent(model::QuestEnv& env, std::span<const int32_t> npcIds, int32_t startVar, bool reward, int32_t varNum) {
	AION_UNPORTED();
}

bool AbstractQuestHandler::defaultOnKillRankedEvent(model::QuestEnv& env, int32_t startVar, int32_t endVar, bool reward) {
	AION_UNPORTED();
}

bool AbstractQuestHandler::defaultOnKillRankedEvent(model::QuestEnv& env, int32_t startVar, int32_t endVar, bool reward, bool isDataDriven) {
	AION_UNPORTED();
}

bool AbstractQuestHandler::defaultOnKillInZoneEvent(model::QuestEnv& env, int32_t startVar, int32_t endVar, bool reward) {
	AION_UNPORTED();
}

bool AbstractQuestHandler::defaultOnKillInZoneEvent(model::QuestEnv& env, int32_t startVar, int32_t endVar, bool reward, bool isDataDriven) {
	AION_UNPORTED();
}

bool AbstractQuestHandler::defaultOnUseSkillEvent(model::QuestEnv& env, int32_t startVar, int32_t endVar, int32_t varNum) {
	AION_UNPORTED();
}

bool AbstractQuestHandler::defaultStartFollowEvent(model::QuestEnv& env, gameserver::model::gameobjects::Npc& follower, int32_t targetNpcId,
	int32_t step, int32_t nextStep) {
	AION_UNPORTED();
}

bool AbstractQuestHandler::defaultStartFollowEvent(model::QuestEnv& env, gameserver::model::gameobjects::Npc& follower, float x, float y, float z,
	int32_t step, int32_t nextStep) {
	AION_UNPORTED();
}

bool AbstractQuestHandler::defaultFollowEndEvent(model::QuestEnv& env, int32_t step, int32_t nextStep, bool reward, int32_t movie) {
	AION_UNPORTED();
}

bool AbstractQuestHandler::defaultFollowEndEvent(model::QuestEnv& env, int32_t step, int32_t nextStep, bool reward) {
	AION_UNPORTED();
}

bool AbstractQuestHandler::defaultOnGetItemEvent(model::QuestEnv& env, int32_t step, int32_t nextStep, bool reward) {
	AION_UNPORTED();
}

bool AbstractQuestHandler::useQuestObject(model::QuestEnv& env, int32_t step, int32_t nextStep, bool reward, bool die) {
	AION_UNPORTED();
}

bool AbstractQuestHandler::useQuestObject(model::QuestEnv& env, int32_t step, int32_t nextStep, bool reward, int32_t varNum, bool die) {
	AION_UNPORTED();
}

bool AbstractQuestHandler::useQuestObject(model::QuestEnv& env, int32_t step, int32_t nextStep, bool reward, int32_t varNum) {
	AION_UNPORTED();
}

bool AbstractQuestHandler::useQuestObject(model::QuestEnv& env, int32_t step, int32_t nextStep, bool reward, int32_t varNum, int32_t addItemId,
	int32_t addItemCount) {
	AION_UNPORTED();
}

bool AbstractQuestHandler::useQuestObject(model::QuestEnv& env, int32_t step, int32_t nextStep, bool reward, int32_t varNum, int32_t addItemId,
	int32_t addItemCount, int32_t removeItemId, int32_t removeItemCount) {
	AION_UNPORTED();
}

bool AbstractQuestHandler::useQuestObject(model::QuestEnv& env, int32_t step, int32_t nextStep, bool reward, int32_t varNum, int32_t movieId) {
	AION_UNPORTED();
}

bool AbstractQuestHandler::useQuestObject(model::QuestEnv& env, int32_t step, int32_t nextStep, bool reward, int32_t varNum, int32_t addItemId,
	int32_t addItemCount, int32_t removeItemId, int32_t removeItemCount, int32_t movieId, bool dieObject) {
	AION_UNPORTED();
}

bool AbstractQuestHandler::useQuestItem(model::QuestEnv& env, gameserver::model::gameobjects::Item& item, int32_t step, int32_t nextStep,
	bool reward) {
	AION_UNPORTED();
}

bool AbstractQuestHandler::useQuestItem(model::QuestEnv& env, gameserver::model::gameobjects::Item& item, int32_t step, int32_t nextStep, bool reward,
	int32_t addItemId, int32_t addItemCount) {
	AION_UNPORTED();
}

bool AbstractQuestHandler::useQuestItem(model::QuestEnv& env, gameserver::model::gameobjects::Item& item, int32_t step, int32_t nextStep, bool reward,
	int32_t movieId) {
	AION_UNPORTED();
}

bool AbstractQuestHandler::useQuestItem(model::QuestEnv& env, gameserver::model::gameobjects::Item& item, int32_t step, int32_t nextStep, bool reward,
	int32_t addItemId, int32_t addItemCount, int32_t movieId) {
	AION_UNPORTED();
}

// anonymous Runnable at AbstractQuestHandler.java:956 (fieldmap.py --class 'com.aionemu.gameserver.questEngine.handlers.AbstractQuestHandler$1'):
// AbstractQuestHandler_Runnable, scheduled for 3000 ms
bool AbstractQuestHandler::useQuestItem(model::QuestEnv& env, gameserver::model::gameobjects::Item& item, int32_t step, int32_t nextStep, bool reward,
	int32_t addItemId, int32_t addItemCount, int32_t movieId, int32_t varNum) {
	AION_UNPORTED();
}

bool AbstractQuestHandler::defaultOnLevelChangedEvent(gameserver::model::gameobjects::player::Player& player,
	std::initializer_list<int32_t> preQuests) {
	AION_UNPORTED();
}

bool AbstractQuestHandler::defaultOnQuestCompletedEvent(model::QuestEnv& env, std::initializer_list<int32_t> preQuests) {
	AION_UNPORTED();
}

bool AbstractQuestHandler::hasAnyPreQuestFinished(gameserver::model::gameobjects::player::QuestStateList& qsl,
	const gameserver::model::templates::quest::XMLStartCondition* startCondition) {
	AION_UNPORTED();
}

bool AbstractQuestHandler::defaultOnEnterZoneEvent(model::QuestEnv& env, const world::zone::ZoneName* currentZoneName,
	const world::zone::ZoneName* questZoneName) {
	AION_UNPORTED();
}

bool AbstractQuestHandler::sendQuestRewardDialog(model::QuestEnv& env, int32_t rewardNpcId, int32_t reportDialogId) {
	AION_UNPORTED();
}

bool AbstractQuestHandler::sendQuestNoneDialog(model::QuestEnv& env, int32_t startNpcId) {
	AION_UNPORTED();
}

bool AbstractQuestHandler::sendQuestNoneDialog(model::QuestEnv& env, int32_t startNpcId, int32_t dialogPageId) {
	AION_UNPORTED();
}

bool AbstractQuestHandler::sendQuestNoneDialog(model::QuestEnv& env, const gameserver::model::templates::QuestTemplate* template_, int32_t startNpcId,
	int32_t dialogPageId) {
	AION_UNPORTED();
}

bool AbstractQuestHandler::sendQuestNoneDialog(model::QuestEnv& env, int32_t startNpcId, int32_t dialogPageId, int32_t itemId, int32_t itemCout) {
	AION_UNPORTED();
}

bool AbstractQuestHandler::sendQuestNoneDialog(model::QuestEnv& env, int32_t startNpcId, int32_t itemId, int32_t itemCout) {
	AION_UNPORTED();
}

bool AbstractQuestHandler::sendQuestNoneDialog(model::QuestEnv& env, const gameserver::model::templates::QuestTemplate* template_, int32_t startNpcId,
	int32_t dialogPageId, int32_t itemId, int32_t itemCout) {
	AION_UNPORTED();
}

bool AbstractQuestHandler::sendItemCollectingStartDialog(model::QuestEnv& env) {
	AION_UNPORTED();
}

runtime::Ptr<gameserver::model::gameobjects::VisibleObject> AbstractQuestHandler::spawn(int32_t templateId,
	gameserver::model::gameobjects::VisibleObject& objectToGetInstanceFrom, float x, float y, float z, int8_t heading) {
	AION_UNPORTED();
}

runtime::Ptr<gameserver::model::gameobjects::VisibleObject> AbstractQuestHandler::spawn(int32_t templateId, world::WorldMapInstance& worldMapInstance,
	float x, float y, float z, int8_t heading) {
	AION_UNPORTED();
}

runtime::Ptr<gameserver::model::gameobjects::VisibleObject> AbstractQuestHandler::spawnInFrontOf(int32_t templateId,
	gameserver::model::gameobjects::VisibleObject& referencePositionObject) {
	AION_UNPORTED();
}

runtime::Ptr<gameserver::model::gameobjects::VisibleObject> AbstractQuestHandler::spawnForFiveMinutesInFrontOf(int32_t templateId,
	gameserver::model::gameobjects::VisibleObject& referencePositionObject, float distance) {
	AION_UNPORTED();
}

runtime::Ptr<gameserver::model::gameobjects::VisibleObject> AbstractQuestHandler::spawnForFiveMinutesInFront(int32_t templateId,
	gameserver::model::gameobjects::VisibleObject& referencePositionObject, int8_t heading, float distance) {
	AION_UNPORTED();
}

runtime::Ptr<gameserver::model::gameobjects::VisibleObject> AbstractQuestHandler::spawnInFront(int32_t templateId,
	world::WorldPosition& referencePosition, std::optional<int8_t> heading, float distance, int32_t timeInMin) {
	AION_UNPORTED();
}

runtime::Ptr<gameserver::model::gameobjects::VisibleObject> AbstractQuestHandler::spawnForFiveMinutes(int32_t templateId,
	world::WorldPosition& position) {
	AION_UNPORTED();
}

runtime::Ptr<gameserver::model::gameobjects::VisibleObject> AbstractQuestHandler::spawnForFiveMinutes(int32_t templateId,
	world::WorldPosition& position, int8_t heading) {
	AION_UNPORTED();
}

runtime::Ptr<gameserver::model::gameobjects::VisibleObject> AbstractQuestHandler::spawnForFiveMinutes(int32_t templateId,
	world::WorldMapInstance& worldMapInstance, float x, float y, float z, int8_t heading) {
	AION_UNPORTED();
}

// lambda at AbstractQuestHandler.java:1287: scheduled deleteIfAliveOrCancelRespawn, pin {&object}
runtime::Ptr<gameserver::model::gameobjects::VisibleObject> AbstractQuestHandler::spawnTemporarily(int32_t templateId,
	world::WorldMapInstance& worldMapInstance, float x, float y, float z, int8_t heading, int32_t timeInMin) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::questEngine::handlers
