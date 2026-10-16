#include "aion/gameserver/handlers/quest/QuestPrelude.h"

#include "aion/gameserver/model/gameobjects/player/QuestStateList.h"
#include "aion/gameserver/model/templates/quest/QuestNpc.h"
#include "aion/gameserver/services/QuestService.h"
#include "aion/gameserver/world/WorldMapInstance.h"

namespace aion::gameserver::handlers::quest::altgard {

// Hand-ported from game-server/data/handlers/quest/altgard/_2252ChasingtheLegend.java (questgen refuses it: WorldMapInstance.getNpcs and
// PacketSendUtility.broadcastMessage are not in its API table), statement by statement in the questgen output's style (docs/deviations/Q10.md,
// Hand ports).

/**
 * @author Ritsu, Majka
 */
class _2252ChasingtheLegend final : public AbstractQuestHandler {
private:
	static constexpr int32_t questStartNpcId = 203646; // Sinood
	static constexpr int32_t questStep1NpcId = 700060; // Bones of Munishan (Npc)
	static constexpr int32_t questActionItemId = 182203235; // Bones of Munishan (Item)
	static constexpr int32_t questKillNpc1Id = 210634; // Minushan's Spirit
	static constexpr int32_t questKillNpc2Id = 210635; // Minushan Drakie. To check on retail if it is spawned also 210635

public:
	_2252ChasingtheLegend() : AbstractQuestHandler(2252) {}

	void register_() override {
		qe.registerQuestNpc(questStartNpcId)->addOnQuestStart(questId); // Sinood
		qe.registerQuestNpc(questStartNpcId)->addOnTalkEvent(questId);
		qe.registerQuestNpc(questStep1NpcId)->addOnTalkEvent(questId); // Bone of Minusha
		qe.registerQuestNpc(questKillNpc1Id)->addOnKillEvent(questId); // Minushan's Spirit
		qe.registerQuestNpc(questKillNpc2Id)->addOnKillEvent(questId); // Minushan Drakie
	}

	bool onDialogEvent(QuestEnv& env) override {
		runtime::Ptr<Player> player = env.getPlayer();
		int32_t targetId = env.getTargetId();
		runtime::Ptr<QuestState> qs = player->getQuestStateList()->getQuestState(questId);
		int32_t dialogActionId = env.getDialogActionId();

		if (qs == nullptr || qs->isStartable()) {
			if (targetId == questStartNpcId) {
				if (dialogActionId == QUEST_SELECT) {
					return sendQuestDialog(env, 1011);
				} else if (dialogActionId == QUEST_ACCEPT_1) {
					if (QuestService::startQuest(env)) {
						giveQuestItem(env, questActionItemId, 1);
						return sendQuestDialog(env, 1003);
					}
				} else {
					return sendQuestStartDialog(env);
				}
			}
		} else if (qs->getStatus() == QuestStatus::START) {
			int32_t var = qs->getQuestVarById(0);
			if (targetId == questStartNpcId) {
				switch (dialogActionId) {
					case QUEST_SELECT:
						if (var == 0) {
							if (giveQuestItem(env, questActionItemId, 1)) { // Player hasn't action item; dialogue for a new chance
								qs->setQuestVarById(0, 0);
								return sendQuestDialog(env, 1693);
							} else { // Dialogue for encouragement
								return sendQuestDialog(env, 2034);
							}
						}
				}
			}
			if (targetId == questStep1NpcId) {
				switch (dialogActionId) {
					case USE_OBJECT:
						if (!player->getWorldMapInstance()->getNpcs({questKillNpc1Id, questKillNpc2Id}).empty())
							return false;

						if (var == 0 && checkItemExistence(env, questActionItemId, 1, true)) {
							// Random spawn
							int32_t chance = 95; // Chance to spawn biggest reward mob
							int32_t spawnTime = 3; // 3 min of spawn
							int32_t questSpawnedNpcId = ::aion::commons::utils::Rnd::chance() < chance ? questKillNpc1Id : questKillNpc2Id;
							runtime::Ptr<VisibleObject> npc = env.getVisibleObject();
							runtime::Ptr<Npc> questMob = runtime::cast<Npc>(spawnTemporarily(questSpawnedNpcId, *npc->getWorldMapInstance(), npc->getX(), npc->getY(), npc->getZ(), npc->getHeading(), spawnTime)); // Minushan's Spirit or Minushan's Drakie
							PacketSendUtility::broadcastMessage(questMob, 1100630, 500);
							// TODO: set not usable icon to questStep1NpcId while mob is spawned, setting usable icon after mob is despawned
						}
				}
			}
		} else if (qs->getStatus() == QuestStatus::REWARD) {
			int32_t var = qs->getQuestVarById(0);

			switch (dialogActionId) {
				case SELECT_QUEST_REWARD:
				case SELECTED_QUEST_NOREWARD:
					qs->setRewardGroup(var - 1);
					return sendQuestEndDialog(env);
				default:
					return sendQuestDialog(env, 1352);
			}
		}
		return false;
	}

	bool onKillEvent(QuestEnv& env) override {
		runtime::Ptr<Player> player = env.getPlayer();
		runtime::Ptr<QuestState> qs = player->getQuestStateList()->getQuestState(questId);
		if (qs == nullptr || qs->getStatus() != QuestStatus::START) {
			return false;
		}

		int32_t var = qs->getQuestVarById(0);
		if (var == 0) {
			int32_t targetId = env.getTargetId();
			switch (targetId) {
				case questKillNpc1Id: // Minushan's Spirit - Highest reward
					qs->setStatus(QuestStatus::REWARD);
					qs->setQuestVarById(0, 1);
					updateQuestStatus(env);
					return true;
				case questKillNpc2Id: // Minushan Drakie - Lowest reward
					qs->setStatus(QuestStatus::REWARD);
					qs->setQuestVarById(0, 2);
					updateQuestStatus(env);
					return true;
			}
		}
		return false;
	}
};
AION_QUEST_HANDLER(_2252ChasingtheLegend, 2252);

} // namespace aion::gameserver::handlers::quest::altgard
