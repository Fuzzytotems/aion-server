#include "aion/gameserver/questEngine/handlers/template/KillSpawned.h"

#include <algorithm>
#include <optional>
#include <string>

#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/SpawnsData.h"
#include "aion/gameserver/model/DialogAction.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/QuestStateList.h"
#include "aion/gameserver/model/templates/QuestTemplate.h"
#include "aion/gameserver/model/templates/quest/QuestNpc.h"
#include "aion/gameserver/model/templates/spawns/SpawnSearchResult.h"
#include "aion/gameserver/model/templates/spawns/SpawnSpotTemplate.h"
#include "aion/gameserver/questEngine/QuestEngine.h"
#include "aion/gameserver/questEngine/handlers/models/Monster.h"
#include "aion/gameserver/questEngine/model/QuestEnv.h"
#include "aion/gameserver/questEngine/model/QuestState.h"
#include "aion/gameserver/questEngine/model/QuestStatus.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/world/WorldMapInstance.h"

namespace aion::gameserver::questEngine::handlers::template_ {

namespace DialogAction = gameserver::model::DialogAction;
using gameserver::model::gameobjects::player::Player;
using model::QuestState;
using model::QuestStatus;
using models::Monster;

namespace {

/** Java `monster.getNpcIds()` dereferenced (NullPointerException for a <monster> without npc_ids) */
const std::vector<int32_t>& npcIdsOf(const Monster& monster) {
	if (!monster.getNpcIds())
		throw runtime::NullPointerException("Monster.npcIds");
	return *monster.getNpcIds();
}

} // namespace

KillSpawned::KillSpawned(int32_t questIdValue, const std::optional<std::vector<int32_t>>& startNpcIdsValue,
	const std::optional<std::vector<int32_t>>& endNpcIdsValue, const std::vector<Monster>& spawnedMonstersValue)
	: AbstractTemplateQuestHandler(questIdValue), isDataDriven(questTemplateOf(questIdValue).isDataDriven()) {
	if (startNpcIdsValue)
		startNpcIds.addAll(*startNpcIdsValue);
	if (endNpcIdsValue)
		endNpcIds.addAll(*endNpcIdsValue);
	else
		endNpcIds.addAll(startNpcIds.snapshot());
	for (const Monster& m : spawnedMonstersValue)
		spawnedMonsters.add(&m);
	for (const Monster* m : spawnedMonsters.snapshot())
		spawnerObjectIds.add(m->getSpawnerNpcId());
}

void KillSpawned::register_() {
	for (int32_t startNpcId : startNpcIds) {
		qe.registerQuestNpc(startNpcId)->addOnQuestStart(questId);
		qe.registerQuestNpc(startNpcId)->addOnTalkEvent(questId);
	}
	if (!equalSets(endNpcIds, startNpcIds)) {
		for (int32_t endNpcId : endNpcIds)
			qe.registerQuestNpc(endNpcId)->addOnTalkEvent(questId);
	}
	for (const Monster* spawnedMonster : spawnedMonsters.snapshot()) {
		for (int32_t spawnedMonsterId : npcIdsOf(*spawnedMonster))
			qe.registerQuestNpc(spawnedMonsterId)->addOnKillEvent(questId);
	}
	for (int32_t spawnerObjectId : spawnerObjectIds)
		qe.registerQuestNpc(spawnerObjectId)->addOnTalkEvent(questId);
}

bool KillSpawned::onDialogEvent(model::QuestEnv& env) {
	runtime::Ptr<Player> player = env.getPlayer();
	runtime::Ptr<QuestState> qs = player->getQuestStateList()->getQuestState(questId);
	int32_t dialogActionId = env.getDialogActionId();
	int32_t targetId = env.getTargetId();

	if (!qs || qs->isStartable()) {
		if (startNpcIds.isEmpty() || startNpcIds.contains(targetId)) {
			if (dialogActionId == DialogAction::QUEST_SELECT) {
				return sendQuestDialog(env, isDataDriven ? 4762 : 1011);
			} else {
				return sendQuestStartDialog(env);
			}
		}
	} else if (qs->getStatus() == QuestStatus::START) {
		if (spawnerObjectIds.contains(targetId)) {
			if (dialogActionId == DialogAction::USE_OBJECT) {
				int32_t monsterId = 0;
				for (const Monster* m : spawnedMonsters.snapshot()) {
					if (m->getSpawnerNpcId() == targetId) {
						const std::vector<int32_t>& npcIds = npcIdsOf(*m);
						if (npcIds.empty()) // Java: List.get(0) of an empty list
							throw runtime::IndexOutOfBoundsException("Index 0 out of bounds for length 0");
						monsterId = npcIds[0];
						break;
					}
				}
				if (monsterId == 0)
					return false;
				std::optional<gameserver::model::templates::spawns::SpawnSearchResult> spawn =
					dataholders::DataManager::SPAWNS_DATA->getFirstSpawnByNpcId(player->getWorldId(), targetId);
				if (!spawn) // Java: NullPointerException on getSpot()
					throw runtime::NullPointerException("SPAWNS_DATA.getFirstSpawnByNpcId(" + std::to_string(player->getWorldId()) + ", " +
						std::to_string(targetId) + ")");
				const gameserver::model::templates::spawns::SpawnSpotTemplate& spot = spawn->getSpot();
				runtime::Ptr<world::WorldMapInstance> instance = player->getWorldMapInstance();
				if (!instance) // Java: NullPointerException in spawnForFiveMinutes
					throw runtime::NullPointerException("player.getWorldMapInstance()");
				spawnForFiveMinutes(monsterId, *instance, spot.getX(), spot.getY(), spot.getZ(), spot.getHeading());
				return true;
			}
		} else {
			for (const Monster* m : spawnedMonsters.snapshot()) {
				if (m->getEndVar() > qs->getQuestVarById(m->getVar())) {
					return false;
				}
			}
			if (endNpcIds.contains(targetId)) {
				if (dialogActionId == DialogAction::QUEST_SELECT) {
					return sendQuestDialog(env, 10002);
				} else if (dialogActionId == DialogAction::SELECT_QUEST_REWARD) {
					return sendQuestDialog(env, 5);
				}
			}
		}
	} else if (qs->getStatus() == QuestStatus::REWARD) {
		if (endNpcIds.contains(targetId)) {
			return sendQuestEndDialog(env);
		}
	}
	return false;
}

bool KillSpawned::onKillEvent(model::QuestEnv& env) {
	runtime::Ptr<Player> player = env.getPlayer();
	runtime::Ptr<QuestState> qs = player->getQuestStateList()->getQuestState(questId);
	if (qs && qs->getStatus() == QuestStatus::START) {
		for (const Monster* m : spawnedMonsters.snapshot()) {
			const std::vector<int32_t>& npcIds = npcIdsOf(*m);
			if (std::ranges::find(npcIds, env.getTargetId()) != npcIds.end()) {
				if (qs->getQuestVarById(m->getVar()) < m->getEndVar()) {
					qs->setQuestVarById(m->getVar(), qs->getQuestVarById(m->getVar()) + 1);
					for (const Monster* n : spawnedMonsters.snapshot()) {
						if (qs->getQuestVarById(n->getVar()) < n->getEndVar()) {
							updateQuestStatus(env);
							return true;
						}
					}
					qs->setStatus(QuestStatus::REWARD);
					updateQuestStatus(env);
					return true;
				}
			}
		}
	}
	return false;
}

} // namespace aion::gameserver::questEngine::handlers::template_
