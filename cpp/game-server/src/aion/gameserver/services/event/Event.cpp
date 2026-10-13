#include "aion/gameserver/services/event/Event.h"

#include "aion/gameserver/configs/Config.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/GuideHtmlData.h"
#include "aion/gameserver/dataholders/QuestsData.h"
#include "aion/gameserver/dataholders/SpawnsData.h"
#include "aion/gameserver/model/TaskId.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/QuestStateList.h"
#include "aion/gameserver/model/templates/Guides/GuideTemplate.h"
#include "aion/gameserver/model/templates/QuestTemplate.h"
#include "aion/gameserver/model/templates/event/Buff.h"
#include "aion/gameserver/model/templates/event/InventoryDrop.h"
#include "aion/gameserver/model/templates/spawns/Spawn.h"
#include "aion/gameserver/model/templates/spawns/SpawnMap.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_QUEST_ACTION.h"
#include "aion/gameserver/questEngine/model/QuestState.h"
#include "aion/gameserver/questEngine/model/QuestStatus.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/sync/Monitor.h"
#include "aion/gameserver/services/QuestService.h"
#include "aion/gameserver/services/RespawnService.h"
#include "aion/gameserver/services/item/ItemPacketService.h"
#include "aion/gameserver/services/item/ItemService.h"
#include "aion/gameserver/spawnengine/SpawnEngine.h"
#include "aion/gameserver/spawnengine/TemporarySpawnEngine.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"
#include "aion/gameserver/utils/time/ServerTime.h"
#include "aion/gameserver/world/World.h"
#include "aion/gameserver/world/WorldMap.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/skillengine/model/Effect_ForceType.h"
#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/model/templates/event/EventTemplate.h"
#include "aion/gameserver/services/event/EventBuffHandler.h"
#include <string>

namespace aion::gameserver::services::event {

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.services.event.Event");

Event::Event(const model::templates::event::EventTemplate* value)
	: eventTemplate(value) {
}

runtime::Ref<Event> Event::create(const model::templates::event::EventTemplate* value) {
	return runtime::makeRef<Event>(value);
}

// Java Event.java:70-72
const skillengine::model::Effect_ForceType* Event::getOrCreateEffectForceType(std::string_view identifier) {
	return skillengine::model::Effect_ForceType::getInstance(std::string(EFFECT_FORCE_TYPE_PREFIX) + std::string(identifier));
}

bool Event::isEventEffectForceType(const skillengine::model::Effect_ForceType* forceType) {
	return forceType != nullptr && forceType->getName().starts_with(EFFECT_FORCE_TYPE_PREFIX);
}

// Java Event.java:78-129. The inventory drop lambda (Event.java:102, fieldmap key event.Event@L102:76) is a fixed-rate task pinning this event.
void Event::start() {
	if (!started.compareAndSet(false, true))
		return;

	if (eventTemplate->hasConfigProperties())
		configs::Config::load();

	if (eventTemplate->getSpawns() != nullptr && eventTemplate->getSpawns()->size() > 0) {
		for (const model::templates::spawns::SpawnMap& map : eventTemplate->getSpawns()->getTemplates()) {
			int8_t difficultId = 0;
			runtime::Ptr<world::WorldMap> worldMap = world::World::getInstance().getWorldMap(map.getMapId());
			for (const std::unique_ptr<model::templates::spawns::Spawn>& spawn : map.getSpawns()) {
				spawn->setEventTemplate(eventTemplate);
				if (spawn->isCustom())
					despawnNonEventSpawns(spawn->getNpcId(), *worldMap);
				if (difficultId == 0)
					difficultId = spawn->getDifficultId();
			}
			dataholders::DataManager::SPAWNS_DATA->addRegularSpawns(map);
			for (const runtime::Ptr<world::WorldMapInstance>& instance : *worldMap)
				spawnengine::SpawnEngine::spawnEventSpawns(*instance, difficultId, 0, eventTemplate);
		}
	}

	const model::templates::event::InventoryDrop* inventoryDrop = eventTemplate->getInventoryDrop();
	if (!inventoryDropTask.get() && inventoryDrop != nullptr) {
		// quiescent-safe: inventoryDrop is static data of the event template (immortal while the server runs)
		inventoryDropTask.set(utils::ThreadPoolManager::getInstance().scheduleAtFixedRate({this}, [inventoryDrop] {
			world::World::getInstance().forEachPlayer([inventoryDrop](model::gameobjects::player::Player& player) {
				if (player.getLevel() >= inventoryDrop->getStartLevel()) {
					// TODO: check the exact type in retail
					runtime::Ref<item::ItemService::ItemUpdatePredicate> predicate = item::ItemService::ItemUpdatePredicate::create(
						item::ItemPacketService::ItemAddType::ITEM_COLLECT, item::ItemPacketService::ItemUpdateType::INC_CASH_ITEM);
					item::ItemService::addItem(player, inventoryDrop->getItemId(), inventoryDrop->getCount(), true, *predicate);
				}
			});
		}, 0, static_cast<int64_t>(inventoryDrop->getInterval() * 60000)));
	}

	if (eventTemplate->getSurveys()) {
		for (const std::string& survey : *eventTemplate->getSurveys()) {
			const model::templates::Guides::GuideTemplate* guideTemplate = dataholders::DataManager::GUIDE_HTML_DATA->getTemplateByTitle(survey);
			if (guideTemplate != nullptr)
				guideTemplate->setActivated(true);
		}
	}

	if (eventTemplate->getBuffs()) {
		std::vector<const model::templates::event::Buff*> buffs;
		for (const model::templates::event::Buff& buff : *eventTemplate->getBuffs())
			buffs.push_back(&buff);
		eventBuffHandler.set(EventBuffHandler::create(eventTemplate->getName(), buffs));
	}

	world::World::getInstance().forEachPlayer([this](model::gameobjects::player::Player& player) { // simulate login on event start
		onPlayerLogin(player);
		onEnterMap(player);
	});
	log.info("Started event: " + eventTemplate->getName());
}

// Java Event.java:131-185
void Event::stop() {
	started.set(false);
	if (eventTemplate->hasConfigProperties()) {
		configs::Config::load();
	}
	if (eventTemplate->getSpawns() != nullptr && eventTemplate->getSpawns()->size() > 0) {
		spawnengine::TemporarySpawnEngine::unregister(eventTemplate);
		int32_t removedCount = 0; // Java: int[] count
		for (const model::templates::spawns::SpawnMap& map : eventTemplate->getSpawns()->getTemplates()) {
			world::World::getInstance().getWorldMap(map.getMapId())->forEachObject([this, &removedCount](model::gameobjects::VisibleObject& o) {
				runtime::Ptr<model::templates::spawns::SpawnTemplate> spawn = o.getSpawn();
				if (spawn && spawn->getEventTemplate() == eventTemplate) { // Java: eventTemplate.equals(...), identity
					o.getController().deleteIfAliveOrCancelRespawn();
					removedCount++;
				}
			});
		}
		removedCount += RespawnService::cancelEventRespawns(eventTemplate);
		dataholders::DataManager::SPAWNS_DATA->removeEventSpawnObjects(*eventTemplate);
		log.info("Removed " + std::to_string(removedCount) + " event spawns (" + eventTemplate->getName() + ")");
	}

	SYNCHRONIZED(*this) {
		// lockdep: onEventEndTasks.get() reads the Field, it is not a Future wait
		if (runtime::Ptr<runtime::RcArrayList<runtime::PinnedCallback<void()>>> tasks = onEventEndTasks.get()) {
			for (const runtime::PinnedCallback<void()>& task : tasks->snapshot()) {
				try {
					task();
				} catch (const std::exception& e) {
					log.error("Could not execute task on end of event " + getEventTemplate()->getName(), e);
				}
			}
			onEventEndTasks.set(nullptr);
		}
	}

	if (runtime::Ptr<runtime::Future> task = inventoryDropTask.get()) {
		task->cancel(false);
		inventoryDropTask.set(nullptr);
	}

	if (eventTemplate->getSurveys()) {
		for (const std::string& survey : *eventTemplate->getSurveys()) {
			const model::templates::Guides::GuideTemplate* guideTemplate = dataholders::DataManager::GUIDE_HTML_DATA->getTemplateByTitle(survey);
			if (guideTemplate != nullptr)
				guideTemplate->setActivated(false);
		}
	}

	if (runtime::Ptr<EventBuffHandler> handler = eventBuffHandler.get()) {
		handler->onEventStop();
		eventBuffHandler.set(nullptr);
	}
	log.info("Stopped event: " + eventTemplate->getName());
}

// Java Event.java:187-189
const skillengine::model::Effect_ForceType* Event::getEffectForceType() {
	runtime::Ptr<EventBuffHandler> handler = eventBuffHandler.get();
	return !handler ? nullptr : handler->getEffectForceType();
}

// Java Event.java:191-194
void Event::onTimeChanged(std::chrono::sys_time<std::chrono::milliseconds> now) {
	runtime::Ptr<EventBuffHandler> handler = eventBuffHandler.get();
	if (handler)
		handler->onTimeChanged(now);
}

// Java Event.java:196-200
void Event::onPlayerLogin(model::gameobjects::player::Player& player) {
	startOrMaintainQuests(player);
	if (!eventTemplate->getLoginMessage().empty()) // Java: != null && !isEmpty() (the bound default is empty)
		utils::PacketSendUtility::sendMessage(player, eventTemplate->getLoginMessage());
}

// Java Event.java:202-205
void Event::onEnteredTeam(model::gameobjects::player::Player& player, model::team::TemporaryPlayerTeam& team) {
	runtime::Ptr<EventBuffHandler> handler = eventBuffHandler.get();
	if (handler)
		handler->onEnteredTeam(player, team);
}

// Java Event.java:207-210
void Event::onLeftTeam(model::gameobjects::player::Player& player, model::team::TemporaryPlayerTeam& team) {
	runtime::Ptr<EventBuffHandler> handler = eventBuffHandler.get();
	if (handler)
		handler->onLeftTeam(player, team);
}

// Java Event.java:212-215
void Event::onEnterMap(model::gameobjects::player::Player& player) {
	runtime::Ptr<EventBuffHandler> handler = eventBuffHandler.get();
	if (handler)
		handler->onEnterMap(player);
}

// Java Event.java:217-220
void Event::onPveKill(model::gameobjects::player::Player& killer, model::gameobjects::Npc& victim) {
	runtime::Ptr<EventBuffHandler> handler = eventBuffHandler.get();
	if (handler)
		handler->onPveKill(killer, victim);
}

// Java Event.java:222-225
void Event::onPvpKill(model::gameobjects::player::Player& killer, model::gameobjects::player::Player& victim) {
	runtime::Ptr<EventBuffHandler> handler = eventBuffHandler.get();
	if (handler)
		handler->onPvpKill(killer, victim);
}

/** Java ServerTime.atDate(qs.getLastCompleteTime()).toLocalDateTime() (a NullPointerException without a complete time) */
static utils::time::ServerTime::LocalDateTime completeTimeOf(questEngine::model::QuestState& qs) {
	const std::optional<commons::database::Timestamp> lastCompleteTime = qs.getLastCompleteTime();
	if (!lastCompleteTime)
		throw runtime::NullPointerException("quest " + std::to_string(qs.getQuestId()) + " has no complete time");
	return utils::time::ServerTime::atDate(*lastCompleteTime).get_local_time();
}

/** Java eventTemplate.getStartDate() dereferenced (a NullPointerException for an event without a start date) */
static const utils::time::ServerTime::LocalDateTime& startDateOf(const model::templates::event::EventTemplate& eventTemplate) {
	if (!eventTemplate.getStartDate())
		throw runtime::NullPointerException("event " + eventTemplate.getName() + " has no start date");
	return *eventTemplate.getStartDate();
}

// Java Event.java:227-257
void Event::startOrMaintainQuests(model::gameobjects::player::Player& player) {
	using network::aion::serverpackets::SM_QUEST_ACTION;
	using questEngine::model::QuestStatus;
	for (int32_t startableQuestId : eventTemplate->getStartableQuests()) {
		if (isAllowedToStartEventQuest(player, startableQuestId)) {
			runtime::Ptr<questEngine::model::QuestState> qs = player.getQuestStateList()->getQuestState(startableQuestId);
			if (!qs) {
				runtime::Ref<questEngine::model::QuestState> created = questEngine::model::QuestState::create(startableQuestId, QuestStatus::START);
				player.getQuestStateList()->addQuest(startableQuestId, *created);
				utils::PacketSendUtility::sendPacket(player, SM_QUEST_ACTION(SM_QUEST_ACTION::ActionType::ADD, *created));
			} else if (qs->getStatus() != QuestStatus::START && qs->getCompleteCount() > 0 && eventTemplate->getStartDate()) {
				const utils::time::ServerTime::LocalDateTime completeTime = completeTimeOf(*qs);
				if (completeTime < *eventTemplate->getStartDate()) { // quest was last completed on a previous event, reset & restart it
					const SM_QUEST_ACTION::ActionType actionType =
						qs->getStatus() == QuestStatus::COMPLETE ? SM_QUEST_ACTION::ActionType::ADD : SM_QUEST_ACTION::ActionType::UPDATE;
					qs->setStatus(QuestStatus::START);
					qs->setQuestVar(0);
					qs->setCompleteCount(0);
					qs->setRewardGroup(std::nullopt);
					utils::PacketSendUtility::sendPacket(player, SM_QUEST_ACTION(actionType, *qs));
				}
			}
		}
	}
	for (int32_t maintainableQuestId : eventTemplate->getMaintainableQuests()) {
		runtime::Ptr<questEngine::model::QuestState> qs = player.getQuestStateList()->getQuestState(maintainableQuestId);
		if (qs && qs->getCompleteCount() > 0 && isAllowedToStartEventQuest(player, maintainableQuestId)) {
			const utils::time::ServerTime::LocalDateTime completeTime = completeTimeOf(*qs);
			if (completeTime < startDateOf(*eventTemplate)) { // quest was last completed on a previous event, reset it
				qs->setCompleteCount(0);
			}
		}
	}
}

// Java Event.java:259-266
bool Event::isAllowedToStartEventQuest(model::gameobjects::player::Player& player, int32_t questId) {
	const model::templates::QuestTemplate* questTemplate = dataholders::DataManager::QUEST_DATA->getQuestById(questId);
	if (questTemplate == nullptr)
		throw runtime::NullPointerException("no quest template " + std::to_string(questId)); // Java: template.getCategory()
	if (questTemplate->getCategory() != model::templates::quest::QuestCategory::EVENT)
		return false;
	if (!QuestService::checkStartConditions(player, questId, false, 0, true, true, false))
		return false;
	return true;
}

// Java Event.java:268-277
void Event::despawnNonEventSpawns(int32_t npcId, world::WorldMap& worldMap) {
	for (const runtime::Ptr<world::WorldMapInstance>& worldMapInstance : worldMap) {
		for (const runtime::Ptr<model::gameobjects::Npc>& npc : worldMapInstance->getNpcs({npcId})) {
			if (npc->getSpawn() && !npc->getSpawn()->isEventSpawn() && !npc->getController().hasScheduledTask(model::TaskId::DECAY)) {
				if (npc->getController().delete_() && !RespawnService::hasRespawnTask(*npc)) {
					runtime::Ref<RespawnService::RespawnTask> respawnTask = RespawnService::RespawnTask::create(*npc);
					RespawnService::RespawnTask& task = *respawnTask;
					addOnEventEndTask(runtime::PinnedCallback<void()>(runtime::Pin{&task}, [&task] { task.run(); }));
				}
			}
		}
	}
}

// Java Event.java:279-288
bool Event::addOnEventEndTask(runtime::PinnedCallback<void()> task) {
	if (!started.get())
		return false;
	SYNCHRONIZED(*this) {
		// lockdep: onEventEndTasks.get() reads the Field, it is not a Future wait
		if (!onEventEndTasks.get())
			onEventEndTasks.set(runtime::RcArrayList<runtime::PinnedCallback<void()>>::create(AION_LOCK_CLASS(Event::onEventEndTasks)));
		// lockdep: onEventEndTasks.get() reads the Field, it is not a Future wait
		onEventEndTasks.get()->add(std::move(task));
		return true;
	}
}

Event::~Event() = default;

} // namespace aion::gameserver::services::event
