#pragma once

#include <any>
#include <cstdint>
#include <initializer_list>
#include <optional>
#include <span>
#include <string_view>
#include <unordered_set>
#include <vector>

#include "aion/gameserver/runtime/collections/Rc.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/model/fwd.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/templates/fwd.h"
#include "aion/gameserver/model/templates/quest/fwd.h"
#include "aion/gameserver/model/templates/rewards/fwd.h"
#include "aion/gameserver/questEngine/fwd.h"
#include "aion/gameserver/questEngine/handlers/HandlerResult.h"
#include "aion/gameserver/questEngine/handlers/fwd.h"
#include "aion/gameserver/questEngine/model/fwd.h"
#include "aion/gameserver/services/item/ItemPacketService_ItemAddType.h"
#include "aion/gameserver/services/item/ItemPacketService_ItemUpdateType.h"
#include "aion/gameserver/world/fwd.h"
#include "aion/gameserver/world/zone/fwd.h"

namespace aion::gameserver::questEngine::handlers {

/**
 * Base of all quest handlers (data/handlers/quest and the XML quest templates): the event hooks and the default quest step helpers.
 * <p>
 * Hub header (docs/design/hub-headers.md). Quest handlers are process-lifetime singletons (handlers-and-porting-plan.md amendment §2):
 * the quest handler registry creates each one with `std::make_unique<C>()` (HandlerRegistry.h QuestFactory, AION_QUEST_HANDLER) and
 * QuestEngine::addQuestHandler keeps it for the life of the process (RT-11). Hence runtime::Immortal (fieldmap.json names RefCounted because
 * an anonymous Runnable of useQuestItem captures `this`; with Immortal the capture is a pinned pointer) and a public virtual destructor.
 * <p>
 * C++ differences:
 * - `qe` (Java: protected static final QuestEngine qe = QuestEngine.getInstance()) is a reference member initialized by the constructor
 *   (handlers-and-porting-plan.md §1.2: no static-initialization side effects), so the ~7,000 `qe.registerX(...)` calls keep their syntax. The
 *   quest prelude includes QuestEngine.h for them.
 * - register() is register_() (keyword rule).
 * - `int[]` parameters are std::span<const int32_t> (call sites pass `std::array{...}` or a static array for Java's `new int[] {...}`);
 *   `int... preQuests` is std::initializer_list<int32_t> with `= {}` for Java's call without varargs; the `Object... objects` of onCanAct
 *   arrive as std::span<const std::any> (QuestEngine::onCanAct passes its arguments on).
 *
 * @author MrPoke, vlog, Majka
 */
// lint: L13 quest handlers are Immortal (amendment §2); L13 accepts their subclasses but not the root class
class AbstractQuestHandler : public runtime::Immortal {
protected:
	QuestEngine& qe; // fieldmap: non-static reference member initialized in the constructor (handlers-and-porting-plan.md §1.2), not a static
	const int32_t questId;
	runtime::Field<runtime::Ref<runtime::RcArrayList<const gameserver::model::templates::quest::QuestItems*>>> workItems{};
	runtime::Field<runtime::Ref<runtime::RcHashSet<int32_t>>> actionItems{};

	/** Create a new AbstractQuestHandler object */
	explicit AbstractQuestHandler(int32_t questId);

public:
	/** Handlers are owned for the life of the process (QuestEngine, RT-11); std::unique_ptr<AbstractQuestHandler> destroys them in tests. */
	virtual ~AbstractQuestHandler();

private:
	void loadWorkItems(const gameserver::model::templates::QuestTemplate* template_);

	void loadActionItems(const gameserver::model::templates::QuestTemplate* template_);

public:
	std::unordered_set<int32_t> getActionItems();

	int32_t getQuestId() const { return questId; }

	virtual void register_() = 0;

	virtual bool onDialogEvent(model::QuestEnv& env);

	/** This method is called on every handler (which registered the event), after a player entered a map. */
	virtual bool onEnterWorldEvent(model::QuestEnv& env) { return false; }

	virtual bool onEnterZoneEvent(model::QuestEnv& env, const world::zone::ZoneName* zoneName) { return false; }

	virtual bool onLeaveZoneEvent(model::QuestEnv& env, const world::zone::ZoneName* zoneName) { return false; }

	virtual HandlerResult onItemUseEvent(model::QuestEnv& env, gameserver::model::gameobjects::Item& item) { return HandlerResult::UNKNOWN; }

	virtual bool onHouseItemUseEvent(model::QuestEnv& env) { return false; }

	virtual bool onGetItemEvent(model::QuestEnv& env) { return false; }

	virtual bool onUseSkillEvent(model::QuestEnv& env, int32_t skillId) { return false; }

	virtual bool onKillEvent(model::QuestEnv& env) { return false; }

	virtual bool onAttackEvent(model::QuestEnv& env) { return false; }

	/** This method is called on every handler (which registered the event), after a player leveled up or down. */
	virtual void onLevelChangedEvent(gameserver::model::gameobjects::player::Player& player) {}

	/** This method is called on every handler (which registered the event), after a quest completed. */
	virtual void onQuestCompletedEvent(model::QuestEnv& env) {}

	virtual bool onDieEvent(model::QuestEnv& env) { return false; }

	virtual bool onLogOutEvent(model::QuestEnv& env) { return false; }

	virtual bool onNpcReachTargetEvent(model::QuestEnv& env) { return false; }

	virtual bool onNpcLostTargetEvent(model::QuestEnv& env) { return false; }

	virtual void onMovieEndEvent(model::QuestEnv& env, int32_t movieId) {}

	virtual bool onQuestTimerEndEvent(model::QuestEnv& env) { return false; }

	virtual bool onInvisibleTimerEndEvent(model::QuestEnv& env) { return false; }

	virtual bool onPassFlyingRingEvent(model::QuestEnv& env, std::string_view flyingRing) { return false; }

	virtual bool onKillRankedEvent(model::QuestEnv& env) { return false; }

	virtual bool onKillInWorldEvent(model::QuestEnv& env) { return false; }

	virtual bool onKillInZoneEvent(model::QuestEnv& env) { return false; }

	virtual bool onFailCraftEvent(model::QuestEnv& env, int32_t itemId) { return false; }

	virtual bool onEquipItemEvent(model::QuestEnv& env, int32_t itemId) { return false; }

	/** objects: Java Object... handed on by QuestEngine::onCanAct (hub-headers.md §7.4) */
	virtual bool onCanAct(model::QuestEnv& env, model::QuestActionType questEventType, std::span<const std::any> objects);

	virtual bool onAddAggroListEvent(model::QuestEnv& env) { return false; }

	virtual bool onAtDistanceEvent(model::QuestEnv& env) { return false; }

	virtual bool onEnterWindStreamEvent(model::QuestEnv& env, int32_t worldId) { return false; }

	bool rideAction(model::QuestEnv& env, int32_t rideItemId) { return false; }

	virtual bool onDredgionRewardEvent(model::QuestEnv& env) { return false; }

	virtual HandlerResult onBonusApplyEvent(model::QuestEnv& env, gameserver::model::templates::rewards::BonusType bonusType,
		const std::vector<const gameserver::model::templates::quest::QuestItems*>& rewardItems) { return HandlerResult::UNKNOWN; }

	bool onProtectEndEvent(model::QuestEnv& env) { return false; }

	bool onProtectFailEvent(model::QuestEnv& env) { return false; }

	/** Update the status of the quest in player's journal */
	void updateQuestStatus(model::QuestEnv& env);

	bool changeQuestStep(model::QuestEnv& env, int32_t oldStep, int32_t newStep);

	bool changeQuestStep(model::QuestEnv& env, int32_t step, int32_t nextStep, bool reward);

	/** Change the quest step to the next step or set quest status to reward */
	bool changeQuestStep(model::QuestEnv& env, int32_t step, int32_t nextStep, bool reward, int32_t varNum);

	/** Send dialog to the player */
	bool sendQuestDialog(model::QuestEnv& env, int32_t dialogPageId);

private:
	void sendDialogPacket(model::QuestEnv& env, int32_t dialogPageId, int32_t questId);

public:
	bool sendQuestSelectionDialog(model::QuestEnv& env);

	bool closeDialogWindow(model::QuestEnv& env);

	bool sendQuestStartDialog(model::QuestEnv& env);

	bool sendQuestStartDialog(model::QuestEnv& env, const gameserver::model::templates::quest::QuestItems* workItem);

	/** Send default start quest dialog and start it (give the item on start) */
	bool sendQuestStartDialog(model::QuestEnv& env, int32_t itemId, int64_t itemCount);

	/** Remove all quest items and send and finish the quest */
	bool sendQuestEndDialog(model::QuestEnv& env, std::span<const int32_t> questItemsToRemove);

	/** Sends reward selection dialog of the quest or finishes it (if selection dialog was active) */
	bool sendQuestEndDialog(model::QuestEnv& env);

private:
	bool isAcceptableQuest(const gameserver::model::templates::QuestTemplate* quest);

public:
	bool defaultCloseDialog(model::QuestEnv& env, int32_t step, int32_t nextStep);

	bool defaultCloseDialog(model::QuestEnv& env, int32_t step, int32_t nextStep, int32_t giveItemId, int64_t giveItemCount);

	bool defaultCloseDialog(model::QuestEnv& env, int32_t step, int32_t nextStep, bool reward, bool sameNpc);

	bool defaultCloseDialog(model::QuestEnv& env, int32_t step, int32_t nextStep, bool reward, bool sameNpc,
		const gameserver::model::templates::quest::QuestItems* questItemToAdd);

	bool defaultCloseDialog(model::QuestEnv& env, int32_t step, int32_t nextStep, int32_t giveItemId, int64_t giveItemCount, int32_t removeItemId,
		int64_t removeItemCount);

	/** Handle on close dialog event, changing the quest status and giving/removing quest items */
	bool defaultCloseDialog(model::QuestEnv& env, int32_t step, int32_t nextStep, bool reward, bool sameNpc, int32_t giveItemId, int64_t giveItemCount,
		int32_t removeItemId, int64_t removeItemCount);

	bool checkQuestItems(model::QuestEnv& env, int32_t step, int32_t nextStep, bool reward, int32_t checkOkId, int32_t checkFailId);

	/** Check if the player has quest item, listed in the quest_data.xml in his inventory */
	bool checkQuestItems(model::QuestEnv& env, int32_t step, int32_t nextStep, bool reward, int32_t checkOkId, int32_t checkFailId, int32_t giveItemId,
		int32_t giveItemCount);

	/** Check if the player has quest item (simple version), listed in the quest_data.xml in his inventory */
	bool checkQuestItemsSimple(model::QuestEnv& env, int32_t step, int32_t nextStep, bool reward, int32_t checkOkId, int32_t giveItemId,
		int32_t giveItemCount);

	/** To use for checking the items, not listed in the collect_items in the quest_data.xml */
	bool checkItemExistence(model::QuestEnv& env, int32_t step, int32_t nextStep, bool reward, int32_t itemId, int32_t itemCount, bool remove,
		int32_t checkOkId, int32_t checkFailId, int32_t giveItemId, int32_t giveItemCount);

	/** Check, if item exists in the player's inventory and probably remove it */
	bool checkItemExistence(model::QuestEnv& env, int32_t itemId, int32_t itemCount, bool remove);

	void sendEmotion(model::QuestEnv& env, gameserver::model::gameobjects::Creature& emoteCreature, gameserver::model::EmotionId emotion,
		bool broadcast);

	/** Give the quest item to player's inventory */
	bool giveQuestItem(model::QuestEnv& env, int32_t itemId, int64_t itemCount);

	bool giveQuestItem(model::QuestEnv& env, int32_t itemId, int64_t itemCount, services::item::ItemPacketService_ItemAddType addType);

	bool giveQuestItem(model::QuestEnv& env, int32_t itemId, int64_t itemCount, services::item::ItemPacketService_ItemAddType addType,
		services::item::ItemPacketService_ItemUpdateType updateType);

	/** Remove the specified count of this quest item from player's inventory */
	bool removeQuestItem(model::QuestEnv& env, int32_t itemId, int64_t itemCount);

	bool removeQuestItem(model::QuestEnv& env, int32_t itemId, int64_t itemCount, model::QuestStatus questStatus);

	bool playQuestMovie(model::QuestEnv& env, int32_t movieId);

	bool playQuestMovie(model::QuestEnv& env, int32_t cutsceneId, bool isCutsceneMovie);

	/** For single kill */
	bool defaultOnKillEvent(model::QuestEnv& env, int32_t npcId, int32_t startVar, int32_t endVar);

	/** For multiple kills */
	bool defaultOnKillEvent(model::QuestEnv& env, std::span<const int32_t> npcIds, int32_t startVar, int32_t endVar);

	/** For single kill on another QuestVar */
	bool defaultOnKillEvent(model::QuestEnv& env, int32_t npcId, int32_t startVar, int32_t endVar, int32_t varNum);

	/** Handle onKill event */
	bool defaultOnKillEvent(model::QuestEnv& env, std::span<const int32_t> npcIds, int32_t startVar, int32_t endVar, int32_t varNum);

	/** For single kill and reward status after it */
	bool defaultOnKillEvent(model::QuestEnv& env, int32_t npcId, int32_t startVar, bool reward);

	/** For single kill on another QuestVar and reward status after it */
	bool defaultOnKillEvent(model::QuestEnv& env, int32_t npcId, int32_t startVar, bool reward, int32_t varNum);

	/** For multiple kills and reward status after it */
	bool defaultOnKillEvent(model::QuestEnv& env, std::span<const int32_t> npcIds, int32_t startVar, bool reward);

	/** Handle onKill event with reward status */
	bool defaultOnKillEvent(model::QuestEnv& env, std::span<const int32_t> npcIds, int32_t startVar, bool reward, int32_t varNum);

	bool defaultOnKillRankedEvent(model::QuestEnv& env, int32_t startVar, int32_t endVar, bool reward);

	bool defaultOnKillRankedEvent(model::QuestEnv& env, int32_t startVar, int32_t endVar, bool reward, bool isDataDriven);

	bool defaultOnKillInZoneEvent(model::QuestEnv& env, int32_t startVar, int32_t endVar, bool reward);

	bool defaultOnKillInZoneEvent(model::QuestEnv& env, int32_t startVar, int32_t endVar, bool reward, bool isDataDriven);

	bool defaultOnUseSkillEvent(model::QuestEnv& env, int32_t startVar, int32_t endVar, int32_t varNum);

	/** NPC starts following the player to the target. Use onLostTarget and onReachTarget for further actions. */
	bool defaultStartFollowEvent(model::QuestEnv& env, gameserver::model::gameobjects::Npc& follower, int32_t targetNpcId, int32_t step,
		int32_t nextStep);

	/**
	 * NPC starts following the player to the target location. Use onLostTarget and onReachTarget for further actions.
	 */
	bool defaultStartFollowEvent(model::QuestEnv& env, gameserver::model::gameobjects::Npc& follower, float x, float y, float z, int32_t step,
		int32_t nextStep);

	/** NPC stops following the player. Used in both onLostTargetEvent and onReachTargetEvent. */
	bool defaultFollowEndEvent(model::QuestEnv& env, int32_t step, int32_t nextStep, bool reward, int32_t movie);

	bool defaultFollowEndEvent(model::QuestEnv& env, int32_t step, int32_t nextStep, bool reward);

	/** Changing quest step on getting item */
	bool defaultOnGetItemEvent(model::QuestEnv& env, int32_t step, int32_t nextStep, bool reward);

	bool useQuestObject(model::QuestEnv& env, int32_t step, int32_t nextStep, bool reward, bool die);

	bool useQuestObject(model::QuestEnv& env, int32_t step, int32_t nextStep, bool reward, int32_t varNum, bool die);

	bool useQuestObject(model::QuestEnv& env, int32_t step, int32_t nextStep, bool reward, int32_t varNum);

	bool useQuestObject(model::QuestEnv& env, int32_t step, int32_t nextStep, bool reward, int32_t varNum, int32_t addItemId, int32_t addItemCount);

	bool useQuestObject(model::QuestEnv& env, int32_t step, int32_t nextStep, bool reward, int32_t varNum, int32_t addItemId, int32_t addItemCount,
		int32_t removeItemId, int32_t removeItemCount);

	bool useQuestObject(model::QuestEnv& env, int32_t step, int32_t nextStep, bool reward, int32_t varNum, int32_t movieId);

	/** Handle use object event */
	bool useQuestObject(model::QuestEnv& env, int32_t step, int32_t nextStep, bool reward, int32_t varNum, int32_t addItemId, int32_t addItemCount,
		int32_t removeItemId, int32_t removeItemCount, int32_t movieId, bool dieObject);

	bool useQuestItem(model::QuestEnv& env, gameserver::model::gameobjects::Item& item, int32_t step, int32_t nextStep, bool reward);

	bool useQuestItem(model::QuestEnv& env, gameserver::model::gameobjects::Item& item, int32_t step, int32_t nextStep, bool reward, int32_t addItemId,
		int32_t addItemCount);

	bool useQuestItem(model::QuestEnv& env, gameserver::model::gameobjects::Item& item, int32_t step, int32_t nextStep, bool reward, int32_t movieId);

	bool useQuestItem(model::QuestEnv& env, gameserver::model::gameobjects::Item& item, int32_t step, int32_t nextStep, bool reward, int32_t addItemId,
		int32_t addItemCount, int32_t movieId);

	/** Handle use item event */
	bool useQuestItem(model::QuestEnv& env, gameserver::model::gameobjects::Item& item, int32_t step, int32_t nextStep, bool reward, int32_t addItemId,
		int32_t addItemCount, int32_t movieId, int32_t varNum);

	/** Starts or locks quest on level up (usually used from campaign quest handlers) */
	bool defaultOnLevelChangedEvent(gameserver::model::gameobjects::player::Player& player, std::initializer_list<int32_t> preQuests = {});

	/** Starts or locks quest after quest completion (usually used from campaign quest handlers). */
	bool defaultOnQuestCompletedEvent(model::QuestEnv& env, std::initializer_list<int32_t> preQuests = {});

private:
	/** Checks recursively if any pre-quest that is required, is completed */
	static bool hasAnyPreQuestFinished(gameserver::model::gameobjects::player::QuestStateList& qsl,
		const gameserver::model::templates::quest::XMLStartCondition* startCondition);

public:
	/** Start a mission on enter the questZone */
	bool defaultOnEnterZoneEvent(model::QuestEnv& env, const world::zone::ZoneName* currentZoneName, const world::zone::ZoneName* questZoneName);

	bool sendQuestRewardDialog(model::QuestEnv& env, int32_t rewardNpcId, int32_t reportDialogId);

	bool sendQuestNoneDialog(model::QuestEnv& env, int32_t startNpcId);

	bool sendQuestNoneDialog(model::QuestEnv& env, int32_t startNpcId, int32_t dialogPageId);

	bool sendQuestNoneDialog(model::QuestEnv& env, const gameserver::model::templates::QuestTemplate* template_, int32_t startNpcId,
		int32_t dialogPageId);

	bool sendQuestNoneDialog(model::QuestEnv& env, int32_t startNpcId, int32_t dialogPageId, int32_t itemId, int32_t itemCout);

	bool sendQuestNoneDialog(model::QuestEnv& env, int32_t startNpcId, int32_t itemId, int32_t itemCout);

	bool sendQuestNoneDialog(model::QuestEnv& env, const gameserver::model::templates::QuestTemplate* template_, int32_t startNpcId,
		int32_t dialogPageId, int32_t itemId, int32_t itemCout);

	bool sendItemCollectingStartDialog(model::QuestEnv& env);

	static runtime::Ptr<gameserver::model::gameobjects::VisibleObject> spawn(int32_t templateId,
		gameserver::model::gameobjects::VisibleObject& objectToGetInstanceFrom, float x, float y, float z, int8_t heading);

	static runtime::Ptr<gameserver::model::gameobjects::VisibleObject> spawn(int32_t templateId, world::WorldMapInstance& worldMapInstance, float x,
		float y, float z, int8_t heading);

	static runtime::Ptr<gameserver::model::gameobjects::VisibleObject> spawnInFrontOf(int32_t templateId,
		gameserver::model::gameobjects::VisibleObject& referencePositionObject);

	static runtime::Ptr<gameserver::model::gameobjects::VisibleObject> spawnForFiveMinutesInFrontOf(int32_t templateId,
		gameserver::model::gameobjects::VisibleObject& referencePositionObject, float distance);

	static runtime::Ptr<gameserver::model::gameobjects::VisibleObject> spawnForFiveMinutesInFront(int32_t templateId,
		gameserver::model::gameobjects::VisibleObject& referencePositionObject, int8_t heading, float distance);

private:
	static runtime::Ptr<gameserver::model::gameobjects::VisibleObject> spawnInFront(int32_t templateId, world::WorldPosition& referencePosition,
		std::optional<int8_t> heading, float distance, int32_t timeInMin);

public:
	static runtime::Ptr<gameserver::model::gameobjects::VisibleObject> spawnForFiveMinutes(int32_t templateId, world::WorldPosition& position);

	static runtime::Ptr<gameserver::model::gameobjects::VisibleObject> spawnForFiveMinutes(int32_t templateId, world::WorldPosition& position,
		int8_t heading);

	static runtime::Ptr<gameserver::model::gameobjects::VisibleObject> spawnForFiveMinutes(int32_t templateId,
		world::WorldMapInstance& worldMapInstance, float x, float y, float z, int8_t heading);

	static runtime::Ptr<gameserver::model::gameobjects::VisibleObject> spawnTemporarily(int32_t templateId, world::WorldMapInstance& worldMapInstance,
		float x, float y, float z, int8_t heading, int32_t timeInMin);
};

} // namespace aion::gameserver::questEngine::handlers
