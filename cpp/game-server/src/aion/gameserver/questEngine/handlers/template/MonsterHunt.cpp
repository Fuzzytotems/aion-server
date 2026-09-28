#include "aion/gameserver/questEngine/handlers/template/MonsterHunt.h"

#include <algorithm>
#include <string>
#include <utility>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/utils/StringUtils.h"
#include "aion/gameserver/configs/administration/AdminConfig.h"
#include "aion/gameserver/model/DialogAction.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/QuestStateList.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/model/rift/RiftLocation.h"
#include "aion/gameserver/model/templates/QuestTemplate.h"
#include "aion/gameserver/model/templates/quest/QuestCategory.h"
#include "aion/gameserver/model/templates/quest/QuestItems.h"
#include "aion/gameserver/model/templates/quest/QuestNpc.h"
#include "aion/gameserver/model/vortex/VortexLocation.h"
#include "aion/gameserver/questEngine/QuestEngine.h"
#include "aion/gameserver/questEngine/model/QuestEnv.h"
#include "aion/gameserver/questEngine/model/QuestState.h"
#include "aion/gameserver/questEngine/model/QuestStatus.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/services/QuestService.h"
#include "aion/gameserver/services/RiftService.h"
#include "aion/gameserver/services/VortexService.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/world/zone/ZoneName.h"

namespace aion::gameserver::questEngine::handlers::template_ {

namespace DialogAction = gameserver::model::DialogAction;
using gameserver::model::gameobjects::player::Player;
using model::QuestState;
using model::QuestStatus;
using models::Monster;

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.questEngine.handlers.template.MonsterHunt");

namespace {

/** Java int arithmetic (two's complement wrap-around; a shift distance is masked by 31) */
constexpr int32_t javaAdd(int32_t a, int32_t b) noexcept {
	return static_cast<int32_t>(static_cast<uint32_t>(a) + static_cast<uint32_t>(b));
}

constexpr int32_t javaSub(int32_t a, int32_t b) noexcept {
	return static_cast<int32_t>(static_cast<uint32_t>(a) - static_cast<uint32_t>(b));
}

constexpr int32_t javaMul(int32_t a, int32_t b) noexcept {
	return static_cast<int32_t>(static_cast<uint32_t>(a) * static_cast<uint32_t>(b));
}

constexpr int32_t javaShl(int32_t a, int32_t distance) noexcept {
	return static_cast<int32_t>(static_cast<uint32_t>(a) << (distance & 31));
}

/** Java `monster.getNpcIds()` dereferenced (NullPointerException for a Monster without ids; register_'s Monsters always have a list) */
const std::vector<int32_t>& npcIdsOf(const Monster& monster) {
	if (!monster.getNpcIds())
		throw runtime::NullPointerException("Monster.npcIds");
	return *monster.getNpcIds();
}

bool containsId(const std::vector<int32_t>& ids, int32_t id) {
	return std::ranges::find(ids, id) != ids.end();
}

/**
 * The loop both hooks repeat (MonsterHunt.java:128-135 and 173-180): the kill count of `monster`, its 6-bit vars from var on, as many vars
 * as `end_var` has 6-bit groups (at least one). `varId` ends one past the last var read.
 */
int32_t killTotal(QuestState& qs, const Monster& monster, int32_t& varId) {
	int32_t endVar = monster.getEndVar();
	varId = monster.getVar();
	int32_t total = 0;
	do {
		int32_t currentVar = qs.getQuestVarById(varId);
		total = javaAdd(total, javaShl(currentVar, javaMul(javaSub(varId, monster.getVar()), 6)));
		endVar >>= 6;
		varId = javaAdd(varId, 1);
	} while (endVar > 0);
	return total;
}

} // namespace

MonsterHunt::MonsterHunt(int32_t questIdValue, const std::optional<std::vector<int32_t>>& startNpcIdsValue,
	const std::optional<std::vector<int32_t>>& endNpcIdsValue, std::vector<Monster> monstersValue, int32_t startDialogIdValue,
	int32_t endDialogIdValue, const std::optional<std::vector<int32_t>>& aggroNpcIdsValue, int32_t invasionWorld, std::string startZoneValue,
	int32_t startDistanceNpcIdValue, bool rewardValue, bool rewardNextStepValue)
	: AbstractTemplateQuestHandler(questIdValue), monsters(std::move(monstersValue)), startDialogId(startDialogIdValue),
	  endDialogId(endDialogIdValue), invasionWorldId(invasionWorld), startZone(std::move(startZoneValue)),
	  startDistanceNpcId(startDistanceNpcIdValue), reward(rewardValue), rewardNextStep(rewardNextStepValue),
	  isDataDriven(questTemplateOf(questIdValue).isDataDriven()) {
	if (startNpcIdsValue)
		startNpcIds.addAll(*startNpcIdsValue);
	if (endNpcIdsValue)
		endNpcIds.addAll(*endNpcIdsValue);
	else
		endNpcIds.addAll(startNpcIds.snapshot());
	if (aggroNpcIdsValue)
		aggroNpcIds.addAll(*aggroNpcIdsValue);
	if (runtime::Ptr<runtime::RcArrayList<const gameserver::model::templates::quest::QuestItems*>> items = workItems.get()) {
		if (items->size() > 1)
			log.warn("Q{} has more than 1 work item", questId);
		workItem = items->get(0);
	}
}

void MonsterHunt::register_() {
	for (int32_t startNpcId : startNpcIds) {
		qe.registerQuestNpc(startNpcId)->addOnQuestStart(questId);
		qe.registerQuestNpc(startNpcId)->addOnTalkEvent(questId);
	}

	for (const Monster& monster : monsters) {
		for (int32_t monsterId : npcIdsOf(monster))
			qe.registerQuestNpc(monsterId)->addOnKillEvent(questId);
	}

	for (int32_t endNpcId : endNpcIds)
		qe.registerQuestNpc(endNpcId)->addOnTalkEvent(questId);

	for (int32_t aggroNpcId : aggroNpcIds)
		qe.registerQuestNpc(aggroNpcId)->addOnAddAggroListEvent(questId);

	if (invasionWorldId != 0)
		qe.registerOnEnterWorld(questId);

	// Java: startZone != null ("" here) and ZoneName.get(startZone) twice, as written (each call warns for a missing zone)
	if (!startZone.empty() && !commons::utils::StringUtils::equalsIgnoreCase(world::zone::ZoneName::get(startZone)->name(), "NONE"))
		qe.registerOnEnterZone(world::zone::ZoneName::get(startZone), questId);

	if (startDistanceNpcId != 0)
		qe.registerQuestNpc(startDistanceNpcId, 300)->addOnAtDistanceEvent(questId);
}

bool MonsterHunt::onDialogEvent(model::QuestEnv& env) {
	runtime::Ptr<Player> player = env.getPlayer();
	runtime::Ptr<QuestState> qs = player->getQuestStateList()->getQuestState(questId);
	int32_t dialogActionId = env.getDialogActionId();
	int32_t targetId = env.getTargetId();

	if (!qs || qs->isStartable()) {
		if (startNpcIds.isEmpty() || startNpcIds.contains(targetId) ||
			questTemplateOf(questId).getCategory() == gameserver::model::templates::quest::QuestCategory::FACTION) {
			switch (dialogActionId) {
				case DialogAction::QUEST_SELECT:
					return sendQuestDialog(env, startDialogId != 0 ? startDialogId : isDataDriven ? 4762 : 1011);
				case DialogAction::QUEST_ACCEPT:
				case DialogAction::QUEST_ACCEPT_1:
				case DialogAction::QUEST_ACCEPT_SIMPLE:
					return sendQuestStartDialog(env, workItem);
				default:
					return AbstractQuestHandler::onDialogEvent(env);
			}
		}
	} else if (qs->getStatus() == QuestStatus::START) {
		if (endNpcIds.contains(targetId)) {
			if (dialogActionId == DialogAction::QUEST_SELECT) {
				return sendQuestDialog(env, endDialogId != 0 ? endDialogId : 1352);
			} else if (dialogActionId == DialogAction::SELECT_QUEST_REWARD) {
				for (const Monster& mi : monsters) {
					int32_t varId = 0;
					int32_t total = killTotal(*qs, mi, varId);
					if (mi.getEndVar() > total) {
						if (player->hasAccess(configs::administration::AdminConfig::DIALOG_INFO.load()))
							utils::PacketSendUtility::sendMessage(*player, "varId: " + std::to_string(varId) + "; req endVar: " +
								std::to_string(mi.getEndVar()) + "; curr total: " + std::to_string(total));
						return false;
					}
				}
				qs->setStatus(QuestStatus::REWARD);
				updateQuestStatus(env);
				return sendQuestDialog(env, 5);
			}
		}
	} else if (qs->getStatus() == QuestStatus::REWARD) {
		if (endNpcIds.contains(targetId)) {
			if (!aggroNpcIds.isEmpty() || isDataDriven) {
				switch (dialogActionId) {
					case DialogAction::QUEST_SELECT:
					case DialogAction::USE_OBJECT:
						return sendQuestDialog(env, 10002);
					case DialogAction::SELECT_QUEST_REWARD:
						if (workItem != nullptr) {
							int64_t currentCount = player->getInventory().getItemCountByItemId(workItem->getItemId());
							if (currentCount > 0)
								removeQuestItem(env, workItem->getItemId(), currentCount, QuestStatus::COMPLETE);
						}
				}
			}
			return sendQuestEndDialog(env);
		}
	}
	return false;
}

bool MonsterHunt::onKillEvent(model::QuestEnv& env) {
	runtime::Ptr<Player> player = env.getPlayer();
	runtime::Ptr<QuestState> qs = player->getQuestStateList()->getQuestState(questId);
	if (qs && qs->getStatus() == QuestStatus::START) {
		int32_t currentTotalVar = 0;
		int32_t totalEndVar = 0;
		int32_t curStep = qs->getQuestVarById(0);
		int32_t lastStep = 0;

		for (const Monster& m : monsters) {
			lastStep = std::max(lastStep, m.getStep());
			if (isDataDriven && m.getStep() != curStep) // Check only for current step for new style quests
				continue;
			if (containsId(npcIdsOf(m), env.getTargetId())) {
				int32_t varId = 0;
				int32_t total = killTotal(*qs, m, varId);
				total = javaAdd(total, 1);
				if (total <= m.getEndVar()) {
					if (!aggroNpcIds.isEmpty()) {
						qs->setStatus(QuestStatus::REWARD);
						updateQuestStatus(env);
						return true;
					} else {
						int32_t tmpTotal = total;
						for (int32_t varsUsed = m.getVar(); varsUsed < varId; varsUsed++) {
							int32_t value = total & 0x3F;
							total >>= 6;
							qs->setQuestVarById(varsUsed, value);
						}
						updateQuestStatus(env);
						if (!isDataDriven) { // Old quest style
							if (tmpTotal == m.getEndVar() && (reward || rewardNextStep)) {
								if (rewardNextStep)
									qs->setQuestVarById(0, javaAdd(qs->getQuestVarById(0), 1));
								qs->setStatus(QuestStatus::REWARD);
								updateQuestStatus(env);
							}
							return true;
						}
					}
				}
			}
			// Totals for quest step
			totalEndVar = javaAdd(totalEndVar, m.getEndVar());
			currentTotalVar = javaAdd(currentTotalVar, qs->getQuestVarById(m.getVar()));
		}

		// Checks if step is completed
		if (currentTotalVar >= totalEndVar && isDataDriven) { // New quest style
			qs->setQuestVar(javaAdd(curStep, 1));
			if (curStep >= lastStep) {
				qs->setStatus(QuestStatus::REWARD);
			}
			updateQuestStatus(env);
			return true;
		}
	}
	return false;
}

bool MonsterHunt::onAddAggroListEvent(model::QuestEnv& env) {
	return startQuest(env);
}

bool MonsterHunt::onEnterWorldEvent(model::QuestEnv& env) {
	runtime::Ptr<Player> player = env.getPlayer();
	runtime::Ptr<QuestState> qs = player->getQuestStateList()->getQuestState(questId);
	runtime::Ptr<gameserver::model::vortex::VortexLocation> vortexLoc = services::VortexService::getInstance().getLocationByWorld(invasionWorldId);
	if (player->getWorldId() == invasionWorldId) {
		if (!qs || qs->isStartable()) {
			if ((vortexLoc && vortexLoc->isActive()) || searchOpenRift())
				return services::QuestService::startQuest(env);
		}
	}
	return false;
}

bool MonsterHunt::searchOpenRift() {
	runtime::Ptr<runtime::RcHashMap<int32_t, runtime::Ref<gameserver::model::rift::RiftLocation>>> locations =
		services::RiftService::getInstance().getRiftLocations();
	if (!locations) // Java: RiftService.locations is null before initRiftLocations
		throw runtime::NullPointerException("RiftService.getRiftLocations()");
	for (const runtime::Ptr<gameserver::model::rift::RiftLocation>& loc : locations->values()) {
		if (loc->getWorldId() == invasionWorldId && loc->isOpened()) {
			return true;
		}
	}
	return false;
}

bool MonsterHunt::onEnterZoneEvent(model::QuestEnv& env, const world::zone::ZoneName* zoneName) {
	if (zoneName == nullptr)
		throw runtime::NullPointerException("zoneName");
	// Java: zoneName.name().equalsIgnoreCase(startZone) is false for a null startZone ("" here)
	if (!startZone.empty() && commons::utils::StringUtils::equalsIgnoreCase(zoneName->name(), startZone))
		return startQuest(env);
	return false;
}

bool MonsterHunt::onAtDistanceEvent(model::QuestEnv& env) {
	return startQuest(env);
}

bool MonsterHunt::startQuest(model::QuestEnv& env) {
	runtime::Ptr<Player> player = env.getPlayer();
	runtime::Ptr<QuestState> qs = player->getQuestStateList()->getQuestState(questId);
	if (!qs || qs->isStartable()) {
		return services::QuestService::startQuest(env);
	}
	return false;
}

} // namespace aion::gameserver::questEngine::handlers::template_
