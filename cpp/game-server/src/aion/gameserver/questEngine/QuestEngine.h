#pragma once

#include <any>
#include <cstdint>
#include <initializer_list>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "aion/gameserver/runtime/collections/ArrayList.h"
#include "aion/gameserver/runtime/collections/HashMap.h"
#include "aion/gameserver/runtime/collections/Rc.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/model/GameEngine.h"
#include "aion/gameserver/model/fwd.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/templates/quest/fwd.h"
#include "aion/gameserver/model/templates/rewards/fwd.h"
#include "aion/gameserver/questEngine/handlers/fwd.h"
#include "aion/gameserver/questEngine/model/fwd.h"
#include "aion/gameserver/utils/stats/fwd.h"
#include "aion/gameserver/world/zone/fwd.h"

namespace aion::gameserver::services::cron {
class JobDetail;
} // namespace aion::gameserver::services::cron

namespace aion::gameserver::questEngine {

/**
 * The quest engine: owns the quest handlers and the event registrations (npc, item, zone, level, ... → quest ids) and dispatches quest events
 * to the registered handlers.
 * <p>
 * Hub header (docs/design/hub-headers.md). An Immortal singleton (§11.2) with Java's instance members. C++ differences:
 * - Java's ScriptManager (runtime compilation of data/handlers and QuestHandlerLoader) has no member: init() creates the handlers from the
 *   quest handler registry (HandlerRegistry.h questHandlerEntries(), handlers-and-porting-plan.md §1.6) and checks each handler's quest id
 *   against its marker.
 * - Quest handlers are Immortal (amendment §2): addQuestHandler takes the factory's std::unique_ptr, and questHandlers holds plain pointers to
 *   handlers that are never freed (RT-11), also after clear().
 * - The event registration methods keep Java's names and parameters, so the ~7,000 `qe.registerX(...)` calls of the handlers keep their syntax.
 *
 * @author MrPoke, Hilgert, vlog, Neon
 */
class QuestEngine : public runtime::Immortal, public gameserver::model::GameEngine {
private:
	runtime::Field<runtime::Ref<services::cron::JobDetail>> messageTask{};
	runtime::HashMap<int32_t, handlers::AbstractQuestHandler*> questHandlers{AION_LOCK_CLASS(QuestEngine::questHandlers)};
	runtime::HashMap<int32_t, runtime::Ref<gameserver::model::templates::quest::QuestNpc>> questNpcs{AION_LOCK_CLASS(QuestEngine::questNpcs)};
	runtime::HashMap<int32_t, runtime::Ref<runtime::RcArrayList<int32_t>>> questItemRelated{AION_LOCK_CLASS(QuestEngine::questItemRelated)};
	runtime::ArrayList<int32_t> questHouseItems{AION_LOCK_CLASS(QuestEngine::questHouseItems)};
	runtime::HashMap<int32_t, runtime::Ref<runtime::RcArrayList<int32_t>>> questItems{AION_LOCK_CLASS(QuestEngine::questItems)};
	runtime::ArrayList<int32_t> questOnCompleted{AION_LOCK_CLASS(QuestEngine::questOnCompleted)};
	runtime::EnumMap<gameserver::model::Race, runtime::Ref<runtime::RcArrayList<int32_t>>> questOnLevelUp{AION_LOCK_CLASS(QuestEngine::questOnLevelUp)};
	runtime::ArrayList<int32_t> questOnDie{AION_LOCK_CLASS(QuestEngine::questOnDie)};
	runtime::ArrayList<int32_t> questOnLogOut{AION_LOCK_CLASS(QuestEngine::questOnLogOut)};
	runtime::ArrayList<int32_t> questOnEnterWorld{AION_LOCK_CLASS(QuestEngine::questOnEnterWorld)};
	runtime::HashMap<const world::zone::ZoneName*, runtime::Ref<runtime::RcArrayList<int32_t>>> questOnEnterZone{
		AION_LOCK_CLASS(QuestEngine::questOnEnterZone)};
	runtime::HashMap<const world::zone::ZoneName*, runtime::Ref<runtime::RcArrayList<int32_t>>> questOnLeaveZone{
		AION_LOCK_CLASS(QuestEngine::questOnLeaveZone)};
	runtime::HashMap<std::string, runtime::Ref<runtime::RcArrayList<int32_t>>> questOnPassFlyingRings{
		AION_LOCK_CLASS(QuestEngine::questOnPassFlyingRings)};
	runtime::ArrayList<int32_t> questOnTimerEnd{AION_LOCK_CLASS(QuestEngine::questOnTimerEnd)};
	// Java: onInvisibleTimerEnd (the name of a method of this class)
	runtime::ArrayList<int32_t> onInvisibleTimerEnd_{AION_LOCK_CLASS(QuestEngine::onInvisibleTimerEnd)};
	runtime::EnumMap<utils::stats::AbyssRankEnum, runtime::Ref<runtime::RcArrayList<int32_t>>> questOnKillRanked{
		AION_LOCK_CLASS(QuestEngine::questOnKillRanked)};
	runtime::HashMap<int32_t, runtime::Ref<runtime::RcArrayList<int32_t>>> questOnKillInWorld{AION_LOCK_CLASS(QuestEngine::questOnKillInWorld)};
	runtime::HashMap<int32_t, runtime::Ref<runtime::RcArrayList<int32_t>>> questOnUseSkill{AION_LOCK_CLASS(QuestEngine::questOnUseSkill)};
	runtime::HashMap<int32_t, int32_t> questOnFailCraft{AION_LOCK_CLASS(QuestEngine::questOnFailCraft)};
	runtime::HashMap<int32_t, runtime::Ref<runtime::RcArrayList<int32_t>>> questOnEquipItem{AION_LOCK_CLASS(QuestEngine::questOnEquipItem)};
	runtime::HashMap<int32_t, runtime::Ref<runtime::RcArrayList<int32_t>>> questCanAct{AION_LOCK_CLASS(QuestEngine::questCanAct)};
	runtime::ArrayList<int32_t> questOnDredgionReward{AION_LOCK_CLASS(QuestEngine::questOnDredgionReward)};
	runtime::EnumMap<gameserver::model::templates::rewards::BonusType, runtime::Ref<runtime::RcArrayList<int32_t>>> questOnBonusApply{
		AION_LOCK_CLASS(QuestEngine::questOnBonusApply)};
	runtime::ArrayList<int32_t> questUpdateItems{AION_LOCK_CLASS(QuestEngine::questUpdateItems)};
	runtime::ArrayList<int32_t> reachTarget{AION_LOCK_CLASS(QuestEngine::reachTarget)};
	runtime::ArrayList<int32_t> lostTarget{AION_LOCK_CLASS(QuestEngine::lostTarget)};
	runtime::ArrayList<int32_t> questOnEnterWindStream{AION_LOCK_CLASS(QuestEngine::questOnEnterWindStream)};
	runtime::ArrayList<int32_t> questRideAction{AION_LOCK_CLASS(QuestEngine::questRideAction)};
	runtime::HashMap<std::string, runtime::Ref<runtime::RcArrayList<int32_t>>> questOnKillInZone{AION_LOCK_CLASS(QuestEngine::questOnKillInZone)};

	QuestEngine();
	~QuestEngine();

public:
	void init() override;

	void reload();

	void clear();

	bool onDialog(model::QuestEnv& env);

	bool onKill(model::QuestEnv& env);

	bool onAttack(model::QuestEnv& env);

	void sendCompletedQuests(gameserver::model::gameobjects::player::Player& player);

	/**
	 * Notifies all quest handlers (which registered the event), that the player level changed
	 */
	void onLevelChanged(gameserver::model::gameobjects::player::Player& player);

	/**
	 * Notifies all quest handlers (which registered the event), that the quest with the specified ID completed
	 */
	void onQuestCompleted(gameserver::model::gameobjects::player::Player& player, int32_t questId);

	void onDie(model::QuestEnv& env);

	void onLogOut(model::QuestEnv& env);

	void onNpcReachTarget(model::QuestEnv& env);

	void onNpcLostTarget(model::QuestEnv& env);

	void onPassFlyingRing(model::QuestEnv& env, std::string_view flyRing);

	void onEnterWorld(gameserver::model::gameobjects::player::Player& player);

	handlers::HandlerResult onItemUseEvent(model::QuestEnv& env, gameserver::model::gameobjects::Item& item);

	void onHouseItemUseEvent(model::QuestEnv& env);

	void onItemGet(gameserver::model::gameobjects::player::Player& player, int32_t itemId);

	void onItemRemoved(gameserver::model::gameobjects::player::Player& player, int32_t itemId);

	bool onKillRanked(model::QuestEnv& env, utils::stats::AbyssRankEnum playerRank);

	bool onKillInWorld(model::QuestEnv& env, int32_t worldId);

	bool onKillInZone(model::QuestEnv& env, std::string_view zoneName);

	bool onEnterZone(model::QuestEnv& env, const world::zone::ZoneName* zoneName);

	bool onLeaveZone(model::QuestEnv& env, const world::zone::ZoneName* zoneName);

	void onMovieEnd(model::QuestEnv& env, int32_t movieId);

	void onQuestTimerEnd(model::QuestEnv& env);

	void onInvisibleTimerEnd(model::QuestEnv& env);

	bool onUseSkill(model::QuestEnv& env, int32_t skillId);

	void onFailCraft(model::QuestEnv& env, int32_t itemId);

	void onEquipItem(model::QuestEnv& env, int32_t itemId);

	/** objects: Java Object... (hub-headers.md §7.4), handed on to AbstractQuestHandler::onCanAct as a span */
	bool onCanAct(model::QuestEnv& env, int32_t templateId, model::QuestActionType questActionType, std::initializer_list<std::any> objects = {});

	void onDredgionReward(model::QuestEnv& env);

	handlers::HandlerResult onBonusApplyEvent(model::QuestEnv& env, gameserver::model::templates::rewards::BonusType bonusType,
		const std::vector<const gameserver::model::templates::quest::QuestItems*>& rewardItems);

	bool onAddAggroList(model::QuestEnv& env);

	bool onAtDistance(model::QuestEnv& env);

	void onEnterWindStream(model::QuestEnv& env, int32_t loc);

	void rideAction(model::QuestEnv& env, int32_t itemId);

	runtime::Ptr<gameserver::model::templates::quest::QuestNpc> registerQuestNpc(int32_t npcId);

	runtime::Ptr<gameserver::model::templates::quest::QuestNpc> registerQuestNpc(int32_t npcId, int32_t range);

	void registerQuestItem(int32_t itemId, int32_t questId);

	bool isRegisteredQuestItem(int32_t itemId);

	void registerQuestHouseItem(int32_t questId);

	void registerOnGetItem(int32_t itemId, int32_t questId);

	void registerOnLevelChanged(int32_t questId);

private:
	/** @return the live list stored in questOnLevelUp for the race (created if absent) */
	runtime::Ptr<runtime::RcArrayList<int32_t>> getOrCreateOnLevelUpForRace(gameserver::model::Race race);

public:
	void registerOnQuestCompleted(int32_t questId);

	void registerOnEnterWorld(int32_t questId);

	void registerOnDie(int32_t questId);

	void registerOnLogOut(int32_t questId);

	void registerOnEnterZone(const world::zone::ZoneName* zoneName, int32_t questId);

	void registerOnKillInZone(std::string_view zone, int32_t questId);

	void registerOnLeaveZone(const world::zone::ZoneName* zoneName, int32_t questId);

	void registerOnKillRanked(utils::stats::AbyssRankEnum playerRank, int32_t questId);

	void registerOnKillInWorld(int32_t worldId, int32_t questId);

	void registerOnPassFlyingRings(std::string_view flyingRing, int32_t questId);

	void registerOnQuestTimerEnd(int32_t questId);

	void registerOnInvisibleTimerEnd(int32_t questId);

	void registerQuestSkill(int32_t skillId, int32_t questId);

	void registerOnFailCraft(int32_t itemId, int32_t questId);

	void registerOnEquipItem(int32_t itemId, int32_t questId);

	bool registerCanAct(int32_t questId, int32_t npcId);

	void registerOnDredgionReward(int32_t questId);

	void registerOnBonusApply(int32_t questId, gameserver::model::templates::rewards::BonusType bonusType);

	void registerAddOnReachTargetEvent(int32_t questId);

	void registerAddOnLostTargetEvent(int32_t questId);

	void registerOnEnterWindStream(int32_t questId);

	void registerOnRide(int32_t questId);

	/** @return the registered QuestNpc, or a new unregistered one (Java: new QuestNpc(npcId)) */
	runtime::Ref<gameserver::model::templates::quest::QuestNpc> getQuestNpc(int32_t npcId);

private:
	/** @return the handler, nullptr if none (handlers are Immortal) */
	handlers::AbstractQuestHandler* getQuestHandlerByQuestId(int32_t questId);

public:
	int32_t getQuestHandlerCount();

	bool isHaveHandler(int32_t questId);

	/**
	 * Registers the handler (created by the quest handler registry) and calls its register_(); a duplicate quest id only logs a warning, like
	 * Java. The engine keeps registered handlers for the life of the process (Immortal, RT-11).
	 */
	void addQuestHandler(std::unique_ptr<handlers::AbstractQuestHandler> questHandler);

	/** Add handler side drop (if not already in xml) */
	void addHandlerSideQuestDrop(int32_t questId, int32_t npcId, int32_t itemId, int32_t amount, int32_t chance);

	void addHandlerSideQuestDrop(int32_t questId, int32_t npcId, int32_t itemId, int32_t amount, int32_t chance, int32_t step);

private:
	void addMessageSendingTask();

public:
	static QuestEngine& getInstance();
};

} // namespace aion::gameserver::questEngine
