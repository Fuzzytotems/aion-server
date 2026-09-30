#include "aion/gameserver/handlers/quest/QuestPrelude.h"

#include "aion/gameserver/controllers/PlayerController.h"
#include "aion/gameserver/model/TaskId.h"
#include "aion/gameserver/model/gameobjects/player/QuestStateList.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/model/templates/quest/QuestNpc.h"
#include "aion/gameserver/services/QuestService.h"

namespace aion::gameserver::handlers::quest::altgard {

// Hand-ported from game-server/data/handlers/quest/altgard/_2230AFriendlyWager.java (questgen refuses it: CreatureController.hasTask and
// cancelTask are not in its API table), statement by statement in the questgen output's style (docs/deviations/Q10.md, Hand ports).

/**
 * @author HellBoy, Majka
 */
class _2230AFriendlyWager final : public AbstractQuestHandler {
private:
	static constexpr int32_t questDropItemId = 182203223; // Mosbear Tusks
	static constexpr int32_t questStartNpcId = 203621; // Shania
	static constexpr int32_t questDurationTime = 1800; // Duration time of the quest 1800

public:
	_2230AFriendlyWager() : AbstractQuestHandler(2230) {}

	void register_() override {
		qe.registerQuestNpc(questStartNpcId)->addOnQuestStart(questId);
		qe.registerQuestNpc(questStartNpcId)->addOnTalkEvent(questId);
		qe.registerOnQuestTimerEnd(questId);
		qe.registerOnLogOut(questId);
	}

	bool onDialogEvent(QuestEnv& env) override {
		runtime::Ptr<Player> player = env.getPlayer();
		int32_t targetId = env.getTargetId();

		runtime::Ptr<QuestState> qs = player->getQuestStateList()->getQuestState(questId);
		int32_t dialogActionId = env.getDialogActionId();

		if (targetId == questStartNpcId) {
			if (qs == nullptr || qs->isStartable()) {
				switch (dialogActionId) {
					case QUEST_ACCEPT_1:
						if (QuestService::startQuest(env)) {
							QuestService::questTimerStart(env, questDurationTime);
							return sendQuestDialog(env, 1003);
						}
						break;
					case QUEST_SELECT:
						return sendQuestDialog(env, 1011);
					default:
						return sendQuestStartDialog(env);
				}
			} else if (qs->getStatus() == QuestStatus::START) {
				switch (dialogActionId) {
					case QUEST_SELECT:
						return sendQuestDialog(env, 2375);
					case SETPRO1:
						if (!player->getController().hasTask(TaskId::QUEST_TIMER)) { // A new chance starts
							QuestService::questTimerStart(env, questDurationTime);
						}
						return sendQuestSelectionDialog(env);
					case CHECK_USER_HAS_QUEST_ITEM:
						// Still time left; check collected items: reward if right number otherwise dialogue to continue
						if (player->getController().hasTask(TaskId::QUEST_TIMER)) {
							if (QuestService::collectItemCheck(env, true)) {
								qs->setStatus(QuestStatus::REWARD);
								updateQuestStatus(env);
								QuestService::questTimerEnd(env);
								return sendQuestDialog(env, 5);
							} else {
								return sendQuestDialog(env, 2716);
							}
						} else { // Time ended; remove quest items and ask for new chance;
							int64_t mosbearTusks = player->getInventory().getItemCountByItemId(questDropItemId);
							removeQuestItem(env, questDropItemId, mosbearTusks);
							return sendQuestDialog(env, 3057);
						}
				}
			} else if (qs->getStatus() == QuestStatus::REWARD) {
				return sendQuestEndDialog(env);
			}
		}
		return false;
	}

	// On time end if not in reward status delete timer task
	bool onQuestTimerEndEvent(QuestEnv& env) override {
		runtime::Ptr<Player> player = env.getPlayer();
		runtime::Ptr<QuestState> qs = player->getQuestStateList()->getQuestState(questId);

		if (qs != nullptr && qs->getStatus() == QuestStatus::START) {
			player->getController().cancelTask(TaskId::QUEST_TIMER);
			return true;
		}
		return false;
	}

	// On logout if not in reward status delete quest items and quest itself
	bool onLogOutEvent(QuestEnv& env) override {
		runtime::Ptr<Player> player = env.getPlayer();
		runtime::Ptr<QuestState> qs = player->getQuestStateList()->getQuestState(questId);

		if (qs != nullptr && qs->getStatus() == QuestStatus::START) {
			int64_t mosbearTusks = player->getInventory().getItemCountByItemId(questDropItemId);
			removeQuestItem(env, questDropItemId, mosbearTusks);
			QuestService::abandonQuest(*player, questId);
			return true;
		}
		return false;
	}
};
AION_QUEST_HANDLER(_2230AFriendlyWager, 2230);

} // namespace aion::gameserver::handlers::quest::altgard
