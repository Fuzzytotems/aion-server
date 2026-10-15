#include "aion/gameserver/handlers/quest/QuestPrelude.h"

#include "aion/gameserver/model/DialogPageInfo.h"
#include "aion/gameserver/model/gameobjects/player/QuestStateList.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/model/templates/quest/QuestNpc.h"

namespace aion::gameserver::handlers::quest::eltnen {

// Hand-ported from game-server/data/handlers/quest/eltnen/_1367MabangtahsFeast.java (questgen refuses it: the fields the handler writes),
// in the questgen output's style, with one correction of the Java code (owner's decision 2026-10-13, both branches; docs/deviations/Q05.md,
// "Correction: 1367 and 24026"): Java keeps the three item counts in fields of the singleton handler (:22), shared by every player, so a
// second player's dialog between one player's count (:48-50) and its removal (:94-96) would remove the second player's counts from the
// first. Here the counts are locals of the dialog, read once per call and passed to removeQuestItems: each player removes their own.

/**
 * @author Atomics
 */
class _1367MabangtahsFeast final : public AbstractQuestHandler {
private:
	static constexpr int32_t ITEM_ID_1 = 182201331;
	static constexpr int32_t ITEM_ID_2 = 182201332;
	static constexpr int32_t ITEM_ID_3 = 182201333;

public:
	_1367MabangtahsFeast() : AbstractQuestHandler(1367) {}

	void register_() override {
		qe.registerQuestNpc(204023)->addOnQuestStart(questId);
		qe.registerQuestNpc(204023)->addOnTalkEvent(questId);
	}

	bool onDialogEvent(QuestEnv& env) override {
		runtime::Ptr<Player> player = env.getPlayer();
		int32_t targetId = 0;
		if (runtime::as<Npc>(env.getVisibleObject()) != nullptr)
			targetId = (runtime::cast<Npc>(env.getVisibleObject()))->getNpcId();
		runtime::Ptr<QuestState> qs = player->getQuestStateList()->getQuestState(questId);
		if (targetId == 204023) {
			if (qs == nullptr || qs->isStartable()) {
				if (env.getDialogActionId() == QUEST_SELECT)
					return sendQuestDialog(env, 1011);
				else
					return sendQuestStartDialog(env);
			} else if (qs->getStatus() == QuestStatus::START) {
				int64_t itemCount1 = player->getInventory().getItemCountByItemId(ITEM_ID_1);
				int64_t itemCount2 = player->getInventory().getItemCountByItemId(ITEM_ID_2);
				int64_t itemCount3 = player->getInventory().getItemCountByItemId(ITEM_ID_3);
				if (env.getDialogActionId() == QUEST_SELECT && qs->getQuestVarById(0) == 0) {
					if (itemCount1 >= 1 || itemCount2 >= 5 || itemCount3 >= 2) {
						return sendQuestDialog(env, 1352);
					} else {
						return sendQuestDialog(env, 1693);
					}
				} else if (env.getDialogActionId() == SETPRO1) {
					if (itemCount1 >= 1) {
						qs->setStatus(QuestStatus::REWARD);
						updateQuestStatus(env);
						removeQuestItems(env, itemCount1, itemCount2, itemCount3);
						qs->setRewardGroup(0);
						return sendQuestDialog(env, ::aion::gameserver::model::id(::aion::gameserver::model::getRewardPageByIndex(qs->getRewardGroup())));
					} else
						return sendQuestDialog(env, 1352);
				} else if (env.getDialogActionId() == SETPRO2) {
					if (itemCount2 >= 5) {
						qs->setStatus(QuestStatus::REWARD);
						updateQuestStatus(env);
						removeQuestItems(env, itemCount1, itemCount2, itemCount3);
						qs->setRewardGroup(1);
						return sendQuestDialog(env, ::aion::gameserver::model::id(::aion::gameserver::model::getRewardPageByIndex(qs->getRewardGroup())));
					} else
						return sendQuestDialog(env, 1352);
				} else if (env.getDialogActionId() == SETPRO3) {
					if (itemCount3 >= 2) {
						qs->setStatus(QuestStatus::REWARD);
						updateQuestStatus(env);
						removeQuestItems(env, itemCount1, itemCount2, itemCount3);
						qs->setRewardGroup(2);
						return sendQuestDialog(env, ::aion::gameserver::model::id(::aion::gameserver::model::getRewardPageByIndex(qs->getRewardGroup())));
					} else
						return sendQuestDialog(env, 1352);
				} else
					return sendQuestStartDialog(env);
			} else if (qs->getStatus() == QuestStatus::REWARD) {
				return sendQuestEndDialog(env);
			}
		}
		return false;
	}

private:
	void removeQuestItems(QuestEnv& env, int64_t itemCount1, int64_t itemCount2, int64_t itemCount3) {
		removeQuestItem(env, ITEM_ID_1, itemCount1);
		removeQuestItem(env, ITEM_ID_2, itemCount2);
		removeQuestItem(env, ITEM_ID_3, itemCount3);
	}
};
AION_QUEST_HANDLER(_1367MabangtahsFeast, 1367);

} // namespace aion::gameserver::handlers::quest::eltnen
