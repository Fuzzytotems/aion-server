#include "aion/gameserver/questEngine/QuestEngine.h"

#include <exception>
#include <memory>
#include <span>
#include <string>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/configs/main/GSConfig.h"
#include "aion/gameserver/controllers/PlayerController.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/NpcData.h"
#include "aion/gameserver/dataholders/QuestsData.h"
#include "aion/gameserver/dataholders/XMLQuests.h"
#include "aion/gameserver/dataholders/loadingutils/EnumTraits.h"
#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/model/Race.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/QuestStateList.h"
#include "aion/gameserver/model/gameobjects/player/npcFaction/NpcFactions.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/model/templates/QuestTemplate.h"
#include "aion/gameserver/model/templates/npc/NpcTemplate.h"
#include "aion/gameserver/model/templates/quest/HandlerSideDrop.h"
#include "aion/gameserver/model/templates/quest/InventoryItem.h"
#include "aion/gameserver/model/templates/quest/InventoryItems.h"
#include "aion/gameserver/model/templates/quest/QuestDrop.h"
#include "aion/gameserver/model/templates/quest/QuestNpc.h"
#include "aion/gameserver/network/aion/serverpackets/SM_QUEST_COMPLETED_LIST.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/questEngine/handlers/AbstractQuestHandler.h"
#include "aion/gameserver/questEngine/handlers/HandlerResult.h"
#include "aion/gameserver/questEngine/model/QuestEnv.h"
#include "aion/gameserver/questEngine/model/QuestState.h"
#include "aion/gameserver/questEngine/model/QuestStatus.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/services/QuestService.h"
#include "aion/gameserver/services/cron/CronService.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/utils/PositionUtil.h"
#include "aion/gameserver/utils/collections/DynamicServerPacketBodySplitList.h"
#include "aion/gameserver/utils/collections/ListPart.h"
#include "aion/gameserver/utils/stats/AbyssRankEnum.h"
#include "aion/gameserver/world/World.h"
#include "aion/gameserver/world/zone/ZoneName.h"

namespace aion::gameserver::questEngine {

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.questEngine.QuestEngine");

namespace {

using gameserver::model::gameobjects::Npc;
using gameserver::model::gameobjects::player::Player;
using handlers::AbstractQuestHandler;
using handlers::HandlerResult;

using IdList = runtime::RcArrayList<int32_t>;

runtime::Ref<IdList> newIdList() {
	return IdList::create();
}

/** Java: (Npc) env.getVisibleObject() (ClassCastException for another object; null stays null) */
runtime::Ptr<Npc> npcOf(model::QuestEnv& env) {
	return runtime::cast<Npc>(env.getVisibleObject());
}

} // namespace

QuestEngine::QuestEngine() = default;

QuestEngine::~QuestEngine() = default;

QuestEngine& QuestEngine::getInstance() {
	static QuestEngine instance; // Java SingletonHolder
	return instance;
}

void QuestEngine::init() {
	for (const gameserver::model::templates::QuestTemplate* data : dataholders::DataManager::QUEST_DATA->getQuestTemplates()) {
		for (const gameserver::model::templates::quest::QuestDrop& drop : data->getQuestDrop()) {
			// Java: drop.setQuestId(data.getId()); C++: the holder set it while loading (templates are const, QuestDrop.h)
			if (!drop.getNpcId())
				throw runtime::NullPointerException("drop.getNpcId()"); // Java: the Integer is unboxed to int
			services::QuestService::addQuestDrop(*drop.getNpcId(), &drop);
		}
		if (data->getInventoryItems() != nullptr) {
			for (const gameserver::model::templates::quest::InventoryItem& inventoryItem : data->getInventoryItems()->getInventoryItems()) {
				// Java adds a null item id too; no int item id ever equals it, so the C++ list (of int) skips it
				const std::optional<int32_t>& itemId = inventoryItem.getItemId();
				if (itemId && !questUpdateItems.contains(*itemId))
					questUpdateItems.add(*itemId);
			}
		}
	}
	// Java: a ScriptManager with a QuestHandlerLoader loads GSConfig.QUEST_HANDLER_DIRECTORY and adds one handler per quest handler class; C++:
	// the handlers are compiled in and listed by the quest handler registry (AION_QUEST_HANDLER markers, HandlerRegistry.h)
	for (const gameserver::handlers::QuestHandlerEntry& entry : gameserver::handlers::questHandlerEntries()) {
		std::unique_ptr<AbstractQuestHandler> handler = entry.create();
		if (handler->getQuestId() != entry.questId) // C++ only: the marker's quest id must be the handler's
			throw runtime::IllegalStateException("Quest handler " + std::string(entry.javaClass) + " has quest id " +
				std::to_string(handler->getQuestId()) + ", but its marker names " + std::to_string(entry.questId));
		addQuestHandler(std::move(handler));
	}
	// Java: for (XMLQuest xmlQuest : DataManager.XML_QUESTS.getAllQuests()) xmlQuest.register(this);
	if (!dataholders::DataManager::XML_QUESTS->getAllQuests().empty())
		AION_PARTIAL("XML quests are not registered: XMLQuest.register and the XML quest handlers are not ported (M5a runs without quests)");
	log.info("Loaded " + std::to_string(questHandlers.size()) + " quest handlers.");
	if (configs::main::GSConfig::ANALYZE_QUESTHANDLERS.load()) {
		// Java: ThreadPoolManager.getInstance().executeLongRunning(() -> QuestSpawnAnalyzer.run(questHandlers.values(), questNpcs.values(), true))
		AION_PARTIAL("QuestSpawnAnalyzer is not ported (gameserver.analysis.quest_handlers)");
	}
	addMessageSendingTask();
}

void QuestEngine::reload() {
	AION_UNPORTED();
}

void QuestEngine::clear() {
	services::cron::CronService::getInstance().cancel(messageTask.get().get());
	services::QuestService::clearQuestDrops();
	questNpcs.clear();
	questItemRelated.clear();
	questItems.clear();
	questHouseItems.clear();
	questOnLevelUp.clear();
	questOnCompleted.clear();
	questOnEnterWorld.clear();
	questOnDie.clear();
	questOnLogOut.clear();
	questOnEnterZone.clear();
	questOnLeaveZone.clear();
	questOnTimerEnd.clear();
	onInvisibleTimerEnd_.clear();
	questOnPassFlyingRings.clear();
	questOnKillRanked.clear();
	questOnKillInWorld.clear();
	questOnKillInZone.clear();
	questOnUseSkill.clear();
	questOnFailCraft.clear();
	questOnEquipItem.clear();
	questCanAct.clear();
	questOnDredgionReward.clear();
	questOnBonusApply.clear();
	questUpdateItems.clear();
	reachTarget.clear();
	lostTarget.clear();
	questOnEnterWindStream.clear();
	questRideAction.clear();
	questHandlers.clear(); // the handlers themselves are Immortal (RT-11)
}

bool QuestEngine::onDialog(model::QuestEnv& env) {
	try {
		AbstractQuestHandler* questHandler;
		if (env.getQuestId() != 0) {
			questHandler = getQuestHandlerByQuestId(env.getQuestId());
			if (questHandler != nullptr) {
				if (questHandler->onDialogEvent(env))
					return true;
				else {
					const gameserver::model::templates::QuestTemplate* qt = dataholders::DataManager::QUEST_DATA->getQuestById(env.getQuestId());
					if (qt != nullptr && qt->getCategory() == gameserver::model::templates::quest::QuestCategory::CHALLENGE_TASK)
						utils::PacketSendUtility::sendPacket(*env.getPlayer(), network::aion::serverpackets::SM_SYSTEM_MESSAGE::STR_MSG_QUEST_LIMIT_START_DAILY(9));
				}
			}
		} else {
			runtime::Ptr<Npc> npc = npcOf(env);
			runtime::Ref<gameserver::model::templates::quest::QuestNpc> questNpc = getQuestNpc(!npc ? 0 : npc->getNpcId());
			for (int32_t questId : questNpc->getOnTalkEvent()) {
				questHandler = getQuestHandlerByQuestId(questId);
				if (questHandler != nullptr) {
					env.setQuestId(questId);
					if (questHandler->onDialogEvent(env))
						return true;
				}
			}
			env.setQuestId(0);
		}
	} catch (const std::exception& ex) {
		log.error("QE: exception in onDialog", ex);
		return false;
	}
	return false;
}

bool QuestEngine::onKill(model::QuestEnv& env) {
	try {
		runtime::Ptr<Npc> npc = npcOf(env);
		runtime::Ref<gameserver::model::templates::quest::QuestNpc> questNpc = getQuestNpc(npc->getNpcId());
		for (int32_t questId : questNpc->getOnKillEvent()) {
			AbstractQuestHandler* questHandler = getQuestHandlerByQuestId(questId);
			if (questHandler != nullptr) {
				env.setQuestId(questId);
				questHandler->onKillEvent(env);
			}
		}
	} catch (const std::exception& ex) {
		log.error("QE: exception in onKill", ex);
		return false;
	}
	return true;
}

bool QuestEngine::onAttack(model::QuestEnv& env) {
	try {
		runtime::Ptr<Npc> npc = npcOf(env);
		runtime::Ref<gameserver::model::templates::quest::QuestNpc> questNpc = getQuestNpc(npc->getNpcId());
		for (int32_t questId : questNpc->getOnAttackEvent()) {
			AbstractQuestHandler* questHandler = getQuestHandlerByQuestId(questId);
			if (questHandler != nullptr) {
				env.setQuestId(questId);
				questHandler->onAttackEvent(env);
			}
		}
	} catch (const std::exception& ex) {
		log.error("QE: exception in onAttack", ex);
		return false;
	}
	return true;
}

void QuestEngine::sendCompletedQuests(gameserver::model::gameobjects::player::Player& player) {
	std::vector<runtime::Ref<model::QuestState>> completedQuests;
	for (const runtime::Ptr<model::QuestState>& questState : player.getQuestStateList()->getCompletedQuests())
		completedQuests.emplace_back(questState);
	utils::collections::DynamicServerPacketBodySplitList<model::QuestState> questStateSplitList(std::move(completedQuests), true,
		network::aion::serverpackets::SM_QUEST_COMPLETED_LIST::STATIC_BODY_SIZE,
		network::aion::serverpackets::SM_QUEST_COMPLETED_LIST::DYNAMIC_BODY_PART_SIZE_CALCULATOR);
	for (utils::collections::ListPart<model::QuestState>& part : questStateSplitList)
		utils::PacketSendUtility::sendPacket(player, network::aion::serverpackets::SM_QUEST_COMPLETED_LIST(part.isFirst() ? 0 : 1, part.borrowed()));
}

void QuestEngine::onLevelChanged(gameserver::model::gameobjects::player::Player& player) {
	try {
		runtime::Ptr<IdList> raceQuestsOnLevelUp = getOrCreateOnLevelUpForRace(player.getRace());
		for (int32_t questId : *raceQuestsOnLevelUp) {
			runtime::Ptr<model::QuestState> qs = player.getQuestStateList()->getQuestState(questId);
			if (!qs || qs->getStatus() != model::QuestStatus::COMPLETE) {
				AbstractQuestHandler* questHandler = getQuestHandlerByQuestId(questId);
				if (questHandler != nullptr)
					questHandler->onLevelChangedEvent(player);
			}
		}
	} catch (const std::exception& ex) {
		log.error("QE: exception in onLevelChanged", ex);
	}
}

void QuestEngine::onQuestCompleted(gameserver::model::gameobjects::player::Player& player, int32_t questId) {
	try {
		runtime::Ref<model::QuestEnv> env = model::QuestEnv::create(nullptr, player, questId);
		for (int32_t onCompletedQuestId : questOnCompleted) {
			AbstractQuestHandler* questHandler = getQuestHandlerByQuestId(onCompletedQuestId);
			if (questHandler != nullptr)
				questHandler->onQuestCompletedEvent(*env);
		}
	} catch (const std::exception& ex) {
		log.error("QE: exception in onQuestCompleted", ex);
	}
}

void QuestEngine::onDie(model::QuestEnv& env) {
	try {
		for (int32_t questId : questOnDie) {
			AbstractQuestHandler* questHandler = getQuestHandlerByQuestId(questId);
			if (questHandler != nullptr) {
				env.setQuestId(questId);
				questHandler->onDieEvent(env);
			}
		}
	} catch (const std::exception& ex) {
		log.error("QE: exception in onDie", ex);
	}
}

void QuestEngine::onLogOut(model::QuestEnv& env) {
	try {
		for (int32_t questId : questOnLogOut) {
			AbstractQuestHandler* questHandler = getQuestHandlerByQuestId(questId);
			if (questHandler != nullptr) {
				env.setQuestId(questId);
				questHandler->onLogOutEvent(env);
			}
		}
	} catch (const std::exception& ex) {
		log.error("QE: exception in onLogOut", ex);
	}
}

void QuestEngine::onNpcReachTarget(model::QuestEnv& env) {
	try {
		for (int32_t questId : reachTarget) {
			AbstractQuestHandler* questHandler = getQuestHandlerByQuestId(questId);
			if (questHandler != nullptr) {
				env.setQuestId(questId);
				questHandler->onNpcReachTargetEvent(env);
			}
		}
	} catch (const std::exception& ex) {
		log.error("QE: exception in onNpcReachTarget", ex);
	}
}

void QuestEngine::onNpcLostTarget(model::QuestEnv& env) {
	try {
		for (int32_t questId : lostTarget) {
			AbstractQuestHandler* questHandler = getQuestHandlerByQuestId(questId);
			if (questHandler != nullptr) {
				env.setQuestId(questId);
				questHandler->onNpcLostTargetEvent(env);
			}
		}
	} catch (const std::exception& ex) {
		log.error("QE: exception in onNpcLostTarget", ex);
	}
}

void QuestEngine::onPassFlyingRing(model::QuestEnv& env, std::string_view flyRing) {
	try {
		runtime::Ptr<IdList> questIds = questOnPassFlyingRings.get(std::string(flyRing));
		if (questIds) {
			for (int32_t questId : *questIds) {
				AbstractQuestHandler* questHandler = getQuestHandlerByQuestId(questId);
				if (questHandler != nullptr) {
					env.setQuestId(questId);
					questHandler->onPassFlyingRingEvent(env, flyRing);
				}
			}
		}
	} catch (const std::exception& ex) {
		log.error("QE: exception in onFlyRingPassEvent", ex);
	}
}

void QuestEngine::onEnterWorld(gameserver::model::gameobjects::player::Player& player) {
	try {
		for (int32_t questId : questOnEnterWorld) {
			AbstractQuestHandler* questHandler = getQuestHandlerByQuestId(questId);
			if (questHandler != nullptr)
				questHandler->onEnterWorldEvent(*model::QuestEnv::create(nullptr, player, questId));
		}
	} catch (const std::exception& ex) {
		log.error("QE: exception in onEnterWorld", ex);
	}
}

handlers::HandlerResult QuestEngine::onItemUseEvent(model::QuestEnv& env, gameserver::model::gameobjects::Item& item) {
	try {
		runtime::Ptr<IdList> questIds = questItemRelated.get(item.getItemId());
		if (questIds) {
			for (int32_t questId : *questIds) {
				AbstractQuestHandler* questHandler = getQuestHandlerByQuestId(questId);
				if (questHandler != nullptr) {
					env.setQuestId(questId);
					HandlerResult result = questHandler->onItemUseEvent(env, item);
					// allow other quests to process, the same item can be used in multiple quests
					if (result != HandlerResult::UNKNOWN)
						return result;
				}
			}
		}
		return HandlerResult::UNKNOWN;
	} catch (const std::exception& ex) {
		log.error("QE: exception in onItemUseEvent", ex);
		return HandlerResult::FAILED;
	}
}

void QuestEngine::onHouseItemUseEvent(model::QuestEnv& env) {
	try {
		for (int32_t questHouseItem : questHouseItems) {
			AbstractQuestHandler* questHandler = getQuestHandlerByQuestId(questHouseItem);
			if (questHandler != nullptr) {
				env.setQuestId(questHouseItem);
				questHandler->onHouseItemUseEvent(env);
			}
		}
	} catch (const std::exception& ex) {
		log.error("QE: exception in onHouseItemUseEvent", ex);
	}
}

void QuestEngine::onItemGet(gameserver::model::gameobjects::player::Player& player, int32_t itemId) {
	runtime::Ptr<IdList> questIds = questItems.get(itemId);
	if (questIds) {
		for (int32_t i = 0; i < questIds->size(); i++) {
			int32_t questId = questItems.get(itemId)->get(i);
			AbstractQuestHandler* questHandler = getQuestHandlerByQuestId(questId);
			if (questHandler != nullptr)
				questHandler->onGetItemEvent(*model::QuestEnv::create(nullptr, player, questId));
		}
	}
	if (questUpdateItems.contains(itemId))
		player.getController().updateNearbyQuests();
}

void QuestEngine::onItemRemoved(gameserver::model::gameobjects::player::Player& player, int32_t itemId) {
	if (questUpdateItems.contains(itemId))
		player.getController().updateNearbyQuests();
}

bool QuestEngine::onKillRanked(model::QuestEnv& env, utils::stats::AbyssRankEnum playerRank) {
	try {
		// Java: if (playerRank != null) (the enum parameter is never null in C++)
		runtime::Ptr<IdList> questIds = questOnKillRanked.get(playerRank);
		if (questIds) {
			for (int32_t questId : *questIds) {
				AbstractQuestHandler* questHandler = getQuestHandlerByQuestId(questId);
				if (questHandler != nullptr) {
					env.setQuestId(questId);
					questHandler->onKillRankedEvent(env);
				}
			}
		}
	} catch (const std::exception& ex) {
		log.error("QE: exception in onKillRanked", ex);
		return false;
	}
	return true;
}

bool QuestEngine::onKillInWorld(model::QuestEnv& env, int32_t worldId) {
	try {
		runtime::Ptr<IdList> questIds = questOnKillInWorld.get(worldId);
		if (questIds) {
			for (int32_t questId : *questIds) {
				AbstractQuestHandler* questHandler = getQuestHandlerByQuestId(questId);
				if (questHandler != nullptr) {
					env.setQuestId(questId);
					questHandler->onKillInWorldEvent(env);
				}
			}
		}
	} catch (const std::exception& ex) {
		log.error("QE: exception in onKillInWorld", ex);
		return false;
	}
	return true;
}

bool QuestEngine::onKillInZone(model::QuestEnv& env, std::string_view zoneName) {
	try {
		runtime::Ptr<IdList> questIds = questOnKillInZone.get(std::string(zoneName));
		if (questIds) {
			for (int32_t questId : *questIds) {
				AbstractQuestHandler* questHandler = getQuestHandlerByQuestId(questId);
				if (questHandler != nullptr) {
					env.setQuestId(questId);
					questHandler->onKillInZoneEvent(env);
				}
			}
		}
	} catch (const std::exception& ex) {
		log.error("QE: exception in onKillInZone", ex);
		return false;
	}
	return true;
}

bool QuestEngine::onEnterZone(model::QuestEnv& env, const world::zone::ZoneName* zoneName) {
	try {
		runtime::Ptr<IdList> questIds = questOnEnterZone.get(zoneName);
		if (questIds) {
			for (int32_t questId : *questIds) {
				AbstractQuestHandler* questHandler = getQuestHandlerByQuestId(questId);
				if (questHandler != nullptr) {
					env.setQuestId(questId);
					questHandler->onEnterZoneEvent(env, zoneName);
				}
			}
		}
	} catch (const std::exception& ex) {
		log.error("QE: exception in onEnterZone", ex);
		return false;
	}
	return true;
}

bool QuestEngine::onLeaveZone(model::QuestEnv& env, const world::zone::ZoneName* zoneName) {
	try {
		runtime::Ptr<IdList> questIds = questOnLeaveZone.get(zoneName);
		if (questIds) {
			for (int32_t questId : *questIds) {
				AbstractQuestHandler* questHandler = getQuestHandlerByQuestId(questId);
				if (questHandler != nullptr) {
					env.setQuestId(questId);
					questHandler->onLeaveZoneEvent(env, zoneName);
				}
			}
		}
	} catch (const std::exception& ex) {
		log.error("QE: exception in onLeaveZone", ex);
		return false;
	}
	return true;
}

void QuestEngine::onMovieEnd(model::QuestEnv& env, int32_t movieId) {
	try {
		AbstractQuestHandler* questHandler = getQuestHandlerByQuestId(env.getQuestId());
		if (questHandler != nullptr)
			questHandler->onMovieEndEvent(env, movieId);
	} catch (const std::exception& ex) {
		log.error("QE: exception in onMovieEnd", ex);
	}
}

void QuestEngine::onQuestTimerEnd(model::QuestEnv& env) {
	try {
		for (int32_t questId : questOnTimerEnd) {
			AbstractQuestHandler* questHandler = getQuestHandlerByQuestId(questId);
			if (questHandler != nullptr) {
				env.setQuestId(questId);
				questHandler->onQuestTimerEndEvent(env);
			}
		}
	} catch (const std::exception& ex) {
		log.error("QE: exception in onQuestTimerEnd", ex);
	}
}

void QuestEngine::onInvisibleTimerEnd(model::QuestEnv& env) {
	try {
		for (int32_t questId : onInvisibleTimerEnd_) {
			AbstractQuestHandler* questHandler = getQuestHandlerByQuestId(questId);
			if (questHandler != nullptr) {
				env.setQuestId(questId);
				questHandler->onInvisibleTimerEndEvent(env);
			}
		}
	} catch (const std::exception& ex) {
		log.error("QE: exception in onInvisibleTimerEnd", ex);
	}
}

bool QuestEngine::onUseSkill(model::QuestEnv& env, int32_t skillId) {
	try {
		runtime::Ptr<IdList> questIds = questOnUseSkill.get(skillId);
		if (questIds) {
			for (int32_t questId : *questIds) {
				AbstractQuestHandler* questHandler = getQuestHandlerByQuestId(questId);
				if (questHandler != nullptr) {
					env.setQuestId(questId);
					questHandler->onUseSkillEvent(env, skillId);
				}
			}
		}
	} catch (const std::exception& ex) {
		log.error("QE: exception in onUseSkill", ex);
		return false;
	}
	return true;
}

void QuestEngine::onFailCraft(model::QuestEnv& env, int32_t itemId) {
	std::optional<int32_t> questId = questOnFailCraft.get(itemId);
	if (questId) {
		AbstractQuestHandler* questHandler = getQuestHandlerByQuestId(*questId);
		if (questHandler != nullptr) {
			if (env.getPlayer()->getInventory().getItemCountByItemId(itemId) == 0) {
				env.setQuestId(*questId);
				questHandler->onFailCraftEvent(env, itemId);
			}
		}
	}
}

void QuestEngine::onEquipItem(model::QuestEnv& env, int32_t itemId) {
	runtime::Ptr<IdList> questIds = questOnEquipItem.get(itemId);
	if (questIds) {
		for (int32_t questId : *questIds) {
			AbstractQuestHandler* questHandler = getQuestHandlerByQuestId(questId);
			if (questHandler != nullptr) {
				env.setQuestId(questId);
				questHandler->onEquipItemEvent(env, itemId);
			}
		}
	}
}

bool QuestEngine::onCanAct(model::QuestEnv& env, int32_t templateId, model::QuestActionType questActionType,
	std::initializer_list<std::any> objects) {
	runtime::Ptr<IdList> questIds = questCanAct.get(templateId);
	if (questIds) {
		for (int32_t questId : *questIds) {
			AbstractQuestHandler* questHandler = getQuestHandlerByQuestId(questId);
			if (questHandler != nullptr) {
				env.setQuestId(questId);
				if (questHandler->onCanAct(env, questActionType, std::span<const std::any>(objects.begin(), objects.size())))
					return true;
			}
		}
	}
	return false;
}

void QuestEngine::onDredgionReward(model::QuestEnv& env) {
	try {
		for (int32_t questId : questOnDredgionReward) {
			AbstractQuestHandler* questHandler = getQuestHandlerByQuestId(questId);
			if (questHandler != nullptr) {
				env.setQuestId(questId);
				questHandler->onDredgionRewardEvent(env);
			}
		}
	} catch (const std::exception& ex) {
		log.error("QE: exception in onDredgionReward", ex);
	}
}

handlers::HandlerResult QuestEngine::onBonusApplyEvent(model::QuestEnv& env, gameserver::model::templates::rewards::BonusType bonusType,
	const std::vector<const gameserver::model::templates::quest::QuestItems*>& rewardItems) {
	try {
		runtime::Ptr<IdList> questIds = questOnBonusApply.get(bonusType);
		if (questIds) {
			for (int32_t questId : *questIds) {
				AbstractQuestHandler* questHandler = getQuestHandlerByQuestId(questId);
				if (questHandler != nullptr) {
					env.setQuestId(questId);
					return questHandler->onBonusApplyEvent(env, bonusType, rewardItems);
				}
			}
		}
		return HandlerResult::UNKNOWN;
	} catch (const std::exception& ex) {
		log.error("QE: exception in onBonusApply", ex);
		return HandlerResult::FAILED;
	}
}

bool QuestEngine::onAddAggroList(model::QuestEnv& env) {
	try {
		runtime::Ptr<Npc> npc = npcOf(env);
		runtime::Ref<gameserver::model::templates::quest::QuestNpc> questNpc = getQuestNpc(npc->getNpcId());
		for (int32_t questId : questNpc->getOnAddAggroListEvent()) {
			AbstractQuestHandler* questHandler = getQuestHandlerByQuestId(questId);
			if (questHandler != nullptr) {
				env.setQuestId(questId);
				questHandler->onAddAggroListEvent(env);
			}
		}
	} catch (const std::exception& ex) {
		log.error("QE: exception in onAddAggroList", ex);
		return false;
	}
	return true;
}

bool QuestEngine::onAtDistance(model::QuestEnv& env) {
	runtime::Ptr<gameserver::model::templates::quest::QuestNpc> questNpc;
	runtime::Ptr<Npc> npc = npcOf(env);
	if (!questNpcs.containsKey(npc->getNpcId())) {
		return false;
	}
	questNpc = getQuestNpc(npc->getNpcId());
	if (getQuestNpc(npc->getNpcId())->getOnDistanceEvent().size() == 0)
		return false;
	runtime::Ptr<Player> player = env.getPlayer();
	if (!utils::PositionUtil::isInRange(*npc, *player, static_cast<float>(questNpc->getQuestRange())))
		return false;
	try {
		for (int32_t questId : questNpc->getOnDistanceEvent()) {
			AbstractQuestHandler* questHandler = getQuestHandlerByQuestId(questId);
			if (questHandler != nullptr) {
				env.setQuestId(questId);
				questHandler->onAtDistanceEvent(env);
			}
		}
	} catch (const std::exception& ex) {
		log.error("QE: exception in onAtDistance", ex);
		return false;
	}
	return true;
}

void QuestEngine::onEnterWindStream(model::QuestEnv& env, int32_t loc) {
	try {
		for (int32_t questId : questOnEnterWindStream) {
			AbstractQuestHandler* questHandler = getQuestHandlerByQuestId(questId);
			if (questHandler != nullptr) {
				env.setQuestId(questId);
				questHandler->onEnterWindStreamEvent(env, loc);
			}
		}
	} catch (const std::exception& ex) {
		log.error("QE: exception in onWindStream", ex);
	}
}

void QuestEngine::rideAction(model::QuestEnv& env, int32_t itemId) {
	try {
		for (int32_t questId : questRideAction) {
			AbstractQuestHandler* questHandler = getQuestHandlerByQuestId(questId);
			if (questHandler != nullptr) {
				env.setQuestId(questId);
				questHandler->rideAction(env, itemId);
			}
		}
	} catch (const std::exception& ex) {
		log.error("QE: exception in rideAction", ex);
	}
}

runtime::Ptr<gameserver::model::templates::quest::QuestNpc> QuestEngine::registerQuestNpc(int32_t npcId) {
	if (!questNpcs.containsKey(npcId)) {
		questNpcs.put(npcId, gameserver::model::templates::quest::QuestNpc::create(npcId));
	}
	return questNpcs.get(npcId);
}

runtime::Ptr<gameserver::model::templates::quest::QuestNpc> QuestEngine::registerQuestNpc(int32_t npcId, int32_t range) {
	if (!questNpcs.containsKey(npcId)) {
		questNpcs.put(npcId, gameserver::model::templates::quest::QuestNpc::create(npcId, range));
	}
	return questNpcs.get(npcId);
}

void QuestEngine::registerQuestItem(int32_t itemId, int32_t questId) {
	questItemRelated.computeIfAbsent(itemId, newIdList)->add(questId);
}

bool QuestEngine::isRegisteredQuestItem(int32_t itemId) {
	return questItemRelated.containsKey(itemId);
}

void QuestEngine::registerQuestHouseItem(int32_t questId) {
	if (!questHouseItems.contains(questId))
		questHouseItems.add(questId);
}

void QuestEngine::registerOnGetItem(int32_t itemId, int32_t questId) {
	questItems.computeIfAbsent(itemId, newIdList)->add(questId);
}

void QuestEngine::registerOnLevelChanged(int32_t questId) {
	const gameserver::model::templates::QuestTemplate* template_ = dataholders::DataManager::QUEST_DATA->getQuestById(questId);
	if (template_ == nullptr)
		throw runtime::NullPointerException("QUEST_DATA.getQuestById(" + std::to_string(questId) + ")");
	std::optional<gameserver::model::Race> racePermitted = template_->getRacePermitted();
	runtime::Ptr<IdList> quests;
	if (!racePermitted) {
		quests = getOrCreateOnLevelUpForRace(gameserver::model::Race::ASMODIANS);
		if (!quests->contains(questId))
			quests->add(questId);
		quests = getOrCreateOnLevelUpForRace(gameserver::model::Race::ELYOS);
		if (!quests->contains(questId))
			quests->add(questId);
	} else {
		quests = getOrCreateOnLevelUpForRace(*racePermitted);
		if (!quests->contains(questId))
			quests->add(questId);
	}
}

runtime::Ptr<runtime::RcArrayList<int32_t>> QuestEngine::getOrCreateOnLevelUpForRace(gameserver::model::Race race) {
	return questOnLevelUp.computeIfAbsent(race, newIdList);
}

void QuestEngine::registerOnQuestCompleted(int32_t questId) {
	if (!questOnCompleted.contains(questId))
		questOnCompleted.add(questId);
}

void QuestEngine::registerOnEnterWorld(int32_t questId) {
	if (!questOnEnterWorld.contains(questId))
		questOnEnterWorld.add(questId);
}

void QuestEngine::registerOnDie(int32_t questId) {
	if (!questOnDie.contains(questId))
		questOnDie.add(questId);
}

void QuestEngine::registerOnLogOut(int32_t questId) {
	if (!questOnLogOut.contains(questId))
		questOnLogOut.add(questId);
}

void QuestEngine::registerOnEnterZone(const world::zone::ZoneName* zoneName, int32_t questId) {
	questOnEnterZone.computeIfAbsent(zoneName, newIdList)->add(questId);
}

void QuestEngine::registerOnKillInZone(std::string_view zone, int32_t questId) {
	questOnKillInZone.computeIfAbsent(std::string(zone), newIdList)->add(questId);
}

void QuestEngine::registerOnLeaveZone(const world::zone::ZoneName* zoneName, int32_t questId) {
	questOnLeaveZone.computeIfAbsent(zoneName, newIdList)->add(questId);
}

void QuestEngine::registerOnKillRanked(utils::stats::AbyssRankEnum playerRank, int32_t questId) {
	const auto& ranks = xml::EnumTraits<utils::stats::AbyssRankEnum>::names;
	for (size_t ordinal = 0; ordinal < ranks.size(); ++ordinal) {
		auto rank = static_cast<utils::stats::AbyssRankEnum>(ordinal);
		if (rank >= playerRank) { // Java: rank.getId() >= playerRank.getId() (getId() is ordinal + 1)
			questOnKillRanked.computeIfAbsent(rank, newIdList)->add(questId);
		}
	}
}

void QuestEngine::registerOnKillInWorld(int32_t worldId, int32_t questId) {
	questOnKillInWorld.computeIfAbsent(worldId, newIdList)->add(questId);
}

void QuestEngine::registerOnPassFlyingRings(std::string_view flyingRing, int32_t questId) {
	questOnPassFlyingRings.computeIfAbsent(std::string(flyingRing), newIdList)->add(questId);
}

void QuestEngine::registerOnQuestTimerEnd(int32_t questId) {
	if (!questOnTimerEnd.contains(questId))
		questOnTimerEnd.add(questId);
}

void QuestEngine::registerOnInvisibleTimerEnd(int32_t questId) {
	if (!onInvisibleTimerEnd_.contains(questId))
		onInvisibleTimerEnd_.add(questId);
}

void QuestEngine::registerQuestSkill(int32_t skillId, int32_t questId) {
	questOnUseSkill.computeIfAbsent(skillId, newIdList)->add(questId);
}

void QuestEngine::registerOnFailCraft(int32_t itemId, int32_t questId) {
	questOnFailCraft.putIfAbsent(itemId, questId);
}

void QuestEngine::registerOnEquipItem(int32_t itemId, int32_t questId) {
	questOnEquipItem.computeIfAbsent(itemId, newIdList)->add(questId);
}

bool QuestEngine::registerCanAct(int32_t questId, int32_t npcId) {
	const gameserver::model::templates::npc::NpcTemplate* template_ = dataholders::DataManager::NPC_DATA->getNpcTemplate(npcId);
	if (template_ == nullptr) {
		log.warn("[QuestEngine] No such NPC template for " + std::to_string(npcId) + " in Q" + std::to_string(questId));
		return false;
	}
	if (template_->getAiName() == "quest_use_item") {
		questCanAct.computeIfAbsent(npcId, newIdList)->add(questId);
		return true;
	}
	return false;
}

void QuestEngine::registerOnDredgionReward(int32_t questId) {
	if (!questOnDredgionReward.contains(questId))
		questOnDredgionReward.add(questId);
}

void QuestEngine::registerOnBonusApply(int32_t questId, gameserver::model::templates::rewards::BonusType bonusType) {
	questOnBonusApply.computeIfAbsent(bonusType, newIdList)->add(questId);
}

void QuestEngine::registerAddOnReachTargetEvent(int32_t questId) {
	if (!reachTarget.contains(questId))
		reachTarget.add(questId);
}

void QuestEngine::registerAddOnLostTargetEvent(int32_t questId) {
	if (!lostTarget.contains(questId))
		lostTarget.add(questId);
}

void QuestEngine::registerOnEnterWindStream(int32_t questId) {
	if (!questOnEnterWindStream.contains(questId))
		questOnEnterWindStream.add(questId);
}

void QuestEngine::registerOnRide(int32_t questId) {
	if (!questRideAction.contains(questId))
		questRideAction.add(questId);
}

runtime::Ref<gameserver::model::templates::quest::QuestNpc> QuestEngine::getQuestNpc(int32_t npcId) {
	runtime::Ptr<gameserver::model::templates::quest::QuestNpc> questNpc = questNpcs.get(npcId);
	if (questNpc) {
		return runtime::Ref<gameserver::model::templates::quest::QuestNpc>(questNpc);
	}
	return gameserver::model::templates::quest::QuestNpc::create(npcId);
}

handlers::AbstractQuestHandler* QuestEngine::getQuestHandlerByQuestId(int32_t questId) {
	return questHandlers.getOrDefault(questId, nullptr);
}

int32_t QuestEngine::getQuestHandlerCount() {
	return questHandlers.size();
}

bool QuestEngine::isHaveHandler(int32_t questId) {
	return questHandlers.containsKey(questId);
}

void QuestEngine::addQuestHandler(std::unique_ptr<handlers::AbstractQuestHandler> questHandler) {
	int32_t questId = questHandler->getQuestId();
	// Immortal handlers (RT-11): the engine keeps every handler it was given, also a duplicate Java drops for the garbage collector
	AbstractQuestHandler* handler = questHandler.release();
	if (questHandlers.putIfAbsent(questId, handler) != nullptr)
		log.warn("Duplicate handler for quest: " + std::to_string(questId));
	else
		handler->register_();
}

void QuestEngine::addHandlerSideQuestDrop(int32_t questId, int32_t npcId, int32_t itemId, int32_t amount, int32_t chance) {
	// never freed: QuestService.questDrop holds template pointers (HandlerSideDrop.h)
	const auto* hsd = new gameserver::model::templates::quest::HandlerSideDrop(questId, npcId, itemId, amount, chance);
	services::QuestService::addQuestDrop(hsd->getNpcId().value(), hsd); // the constructor sets the npc id
}

void QuestEngine::addHandlerSideQuestDrop(int32_t questId, int32_t npcId, int32_t itemId, int32_t amount, int32_t chance, int32_t step) {
	// never freed: QuestService.questDrop holds template pointers (HandlerSideDrop.h)
	const auto* hsd = new gameserver::model::templates::quest::HandlerSideDrop(questId, npcId, itemId, amount, chance, step);
	services::QuestService::addQuestDrop(hsd->getNpcId().value(), hsd); // the constructor sets the npc id
}

// lambda at QuestEngine.java:912 (com.aionemu.gameserver.questEngine.QuestEngine@L912:52): CronService job, captureless
void QuestEngine::addMessageSendingTask() {
	messageTask.set(services::cron::CronService::getInstance().schedule(services::cron::CronJob([] {
		world::World::getInstance().forEachPlayer([](Player& player) {
			bool daily = false, weekly = false;
			for (const runtime::Ptr<model::QuestState>& qs : player.getQuestStateList()->getCompletedQuests()) {
				if (qs->isStartable()) {
					const gameserver::model::templates::QuestTemplate* template_ = dataholders::DataManager::QUEST_DATA->getQuestById(qs->getQuestId());
					if (template_ == nullptr)
						throw runtime::NullPointerException("QUEST_DATA.getQuestById(" + std::to_string(qs->getQuestId()) + ")");
					if (!daily && template_->isDaily())
						daily = true;
					else if (!weekly && template_->isWeekly())
						weekly = true;
					if (daily && weekly)
						break;
				}
			}
			if (daily)
				utils::PacketSendUtility::sendPacket(player, network::aion::serverpackets::SM_SYSTEM_MESSAGE::STR_MSG_QUEST_LIMIT_RESET_DAILY());
			if (weekly)
				utils::PacketSendUtility::sendPacket(player, network::aion::serverpackets::SM_SYSTEM_MESSAGE::STR_MSG_QUEST_LIMIT_RESET_WEEK());
			if (daily || weekly)
				player.getController().updateNearbyQuests();
			player.getNpcFactions().sendDailyQuest();
		});
	}), "0 0 9 ? * *"));
}

} // namespace aion::gameserver::questEngine
