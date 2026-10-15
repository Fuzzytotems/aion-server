#include "aion/gameserver/handlers/quest/QuestPrelude.h"

#include "aion/gameserver/model/gameobjects/player/QuestStateList.h"
#include "aion/gameserver/model/templates/quest/QuestNpc.h"

namespace aion::gameserver::handlers::quest::reshanta {

// Hand-ported from game-server/data/handlers/quest/reshanta/_2759TenaciousGuardian.java (questgen refuses it: the List<Integer> field), in
// the questgen output's style, with one correction of the Java code (owner's decision 2026-10-05, both branches; docs/deviations/Q01.md,
// "_2759TenaciousGuardian"): Java's `killedMobs` (:19) is one list in the singleton handler, shared by every player, so one player's kill of
// a guardian keeps every other player from counting that guardian until somebody takes the reward (:57, :76-94). Here each player's record
// of the three guardians is their own, in the quest's var slot 1 (KILL_RECORD_SLOT, one bit per guardian): a guardian counts once per player
// and never blocks another player. Var slot 0 counts the kills as in Java.

/**
 * @author vlog
 */
class _2759TenaciousGuardian final : public AbstractQuestHandler {
public:
	_2759TenaciousGuardian() : AbstractQuestHandler(2759) {}

	void register_() override {
		qe.registerQuestNpc(264769)->addOnQuestStart(questId);
		qe.registerQuestNpc(264769)->addOnTalkEvent(questId);
		qe.registerQuestNpc(278588)->addOnKillEvent(questId);
		qe.registerQuestNpc(278589)->addOnKillEvent(questId);
		qe.registerQuestNpc(278590)->addOnKillEvent(questId);
	}

	bool onDialogEvent(QuestEnv& env) override {
		runtime::Ptr<Player> player = env.getPlayer();
		runtime::Ptr<QuestState> qs = player->getQuestStateList()->getQuestState(questId);
		int32_t dialogActionId = env.getDialogActionId();
		int32_t targetId = env.getTargetId();

		if (qs == nullptr || qs->isStartable()) {
			if (targetId == 264769) { // Gudharten
				if (dialogActionId == QUEST_SELECT) {
					return sendQuestDialog(env, 1011);
				} else {
					return sendQuestStartDialog(env);
				}
			}
		} else if (qs->getStatus() == QuestStatus::START) {
			int32_t var = qs->getQuestVarById(0);
			if (targetId == 264769) { // Gudharten
				if (dialogActionId == QUEST_SELECT) {
					if (var == 3) {
						return sendQuestDialog(env, 1352);
					}
				} else if (dialogActionId == SELECT_QUEST_REWARD) {
					// Java (:56-57): changeQuestStep, then killedMobs.clear() also when the step did not change (var < 3), which would let
					// a guardian count twice; the record is cleared only with the reward, before the step's update packet
					if (var == 3)
						qs->setQuestVarById(KILL_RECORD_SLOT, 0);
					changeQuestStep(env, 3, 3, true); // reward
					return sendQuestDialog(env, 5);
				}
			}
		} else if (qs->getStatus() == QuestStatus::REWARD) {
			if (targetId == 264769) { // Gudharten
				return sendQuestEndDialog(env);
			}
		}
		return false;
	}

	bool onKillEvent(QuestEnv& env) override {
		runtime::Ptr<Player> player = env.getPlayer();
		runtime::Ptr<QuestState> qs = player->getQuestStateList()->getQuestState(questId);
		int32_t targetId = env.getTargetId();
		if (qs != nullptr && qs->getStatus() == QuestStatus::START) {
			int32_t var = qs->getQuestVarById(0);
			if (var < 3) {
				int32_t bit = guardianBit(targetId);
				int32_t killed = qs->getQuestVarById(KILL_RECORD_SLOT);
				if (bit != 0 && (killed & bit) == 0) { // Java: !killedMobs.contains(id), then killedMobs.add(id) (:78-93)
					qs->setQuestVarById(KILL_RECORD_SLOT, killed | bit);
					return defaultOnKillEvent(env, targetId, var, var + 1);
				}
			}
		}
		return false;
	}

private:
	/** the quest var slot that records the guardians this player killed (slot 0 counts them) */
	static constexpr int32_t KILL_RECORD_SLOT = 1;

	/** 278588, 278589 and 278590 as the bits 1, 2 and 4 of the record; 0 for any other npc */
	static int32_t guardianBit(int32_t npcId) {
		switch (npcId) {
			case 278588:
				return 1;
			case 278589:
				return 2;
			case 278590:
				return 4;
			default:
				return 0;
		}
	}
};
AION_QUEST_HANDLER(_2759TenaciousGuardian, 2759);

} // namespace aion::gameserver::handlers::quest::reshanta
