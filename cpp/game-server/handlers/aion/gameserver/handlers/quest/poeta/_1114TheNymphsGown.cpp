#include "aion/gameserver/handlers/quest/QuestPrelude.h"

#include <array>

#include "aion/gameserver/controllers/attack/AggroList.h"
#include "aion/gameserver/model/gameobjects/player/QuestStateList.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/model/templates/quest/QuestNpc.h"
#include "aion/gameserver/network/aion/serverpackets/SM_DIALOG_WINDOW.h"
#include "aion/gameserver/services/QuestService.h"
#include "aion/gameserver/world/knownlist/KnownList.h"

namespace aion::gameserver::handlers::quest::poeta {

// Hand-ported from game-server/data/handlers/quest/poeta/_1114TheNymphsGown.java (questgen refuses it: the lambda of :115), statement by
// statement in the questgen output's style (docs/deviations/Q05-hand.md).

/**
 * @author Rhys2002
 */
class _1114TheNymphsGown final : public AbstractQuestHandler {
private:
	// fieldmap: a table of literals that is never written (Java's effectively final int[]), questgen's static constexpr std::array
	static constexpr std::array<int32_t, 3> npc_ids{203075, 203058, 700008};

public:
	_1114TheNymphsGown() : AbstractQuestHandler(1114) {}

	void register_() override {
		qe.registerQuestItem(182200214, questId);
		for (int32_t npc_id : npc_ids)
			qe.registerQuestNpc(npc_id)->addOnTalkEvent(questId);
	}

	bool onDialogEvent(QuestEnv& env) override {
		runtime::Ptr<Player> player = env.getPlayer();
		int32_t targetId = 0;
		runtime::Ptr<QuestState> qs = player->getQuestStateList()->getQuestState(questId);
		if (runtime::as<Npc>(env.getVisibleObject()) != nullptr)
			targetId = (runtime::cast<Npc>(env.getVisibleObject()))->getNpcId();

		if (targetId == 0) {
			if (qs == nullptr || qs->isStartable()) {
				if (env.getDialogActionId() == QUEST_ACCEPT_1) {
					QuestService::startQuest(env);
					giveQuestItem(env, 182200226, 1);
					removeQuestItem(env, 182200214, 1); // Namus's Diary (which started the quest via double-click)
					PacketSendUtility::sendPacket(*player, SM_DIALOG_WINDOW(0, 0));
					return true;
				} else
					PacketSendUtility::sendPacket(*player, SM_DIALOG_WINDOW(0, 0));
			}
		}

		if (qs == nullptr)
			return false;

		int32_t var = qs->getQuestVarById(0);

		if (qs->getStatus() == QuestStatus::REWARD) {
			if (targetId == 203075 && var == 4) { // Namus
				if (env.getDialogActionId() == USE_OBJECT)
					return sendQuestDialog(env, 2375);
				else if (env.getDialogActionId() == SELECT_QUEST_REWARD)
					return sendQuestDialog(env, 6);
				else
					return sendQuestEndDialog(env);
			} else if (targetId == 203058 && var == 3) // Asteros
				return sendQuestEndDialog(env);
		} else if (qs->getStatus() != QuestStatus::START)
			return false;

		if (targetId == 203075) { // Namus
			switch (env.getDialogActionId()) {
				case QUEST_SELECT:
					if (var == 0)
						return sendQuestDialog(env, 1011);
					else if (var == 2)
						return sendQuestDialog(env, 1693);
					else if (var == 3)
						return sendQuestDialog(env, 2375);
					return false;
				case SELECT_QUEST_REWARD:
					if (var == 2 || var == 3) {
						qs->setQuestVarById(0, 4);
						qs->setStatus(QuestStatus::REWARD);
						qs->setRewardGroup(1);
						updateQuestStatus(env);
						removeQuestItem(env, 182200217, 1);
						return sendQuestDialog(env, 6);
					}
					return false;
				case SETPRO1:
					if (var == 0) {
						qs->setQuestVarById(0, var + 1);
						updateQuestStatus(env);
						removeQuestItem(env, 182200226, 1);
						PacketSendUtility::sendPacket(*player, SM_DIALOG_WINDOW(env.getVisibleObject()->getObjectId(), 10));
						return true;
					}
					return false;
				case SETPRO2:
					if (var == 2) {
						qs->setQuestVarById(0, var + 1);
						updateQuestStatus(env);
						PacketSendUtility::sendPacket(*player, SM_DIALOG_WINDOW(env.getVisibleObject()->getObjectId(), 10));
						return true;
					}
			}
		} else if (targetId == 700008) { // Seirenia's clothes
			switch (env.getDialogActionId()) {
				case USE_OBJECT:
					if (var == 1) {
						// lambda at _1114TheNymphsGown.java:115: a synchronous KnownList visitor (forEachNpc runs it before it returns)
						Player& wearer = *player;
						player->getKnownList().forEachNpc([&wearer](Npc& npc) {
							if (npc.getNpcId() != 203175) // Seirenia
								return;
							npc.getAggroList().addHate(wearer, 50);
						});
						giveQuestItem(env, 182200217, 1); // Nymph's Dress
						qs->setQuestVarById(0, 2);
						updateQuestStatus(env);
					}
					return true;
			}
		}
		if (targetId == 203058) { // Asteros
			switch (env.getDialogActionId()) {
				case QUEST_SELECT:
					if (var == 3)
						return sendQuestDialog(env, 2034);
					return false;
				case SETPRO3:
					if (var == 3) {
						qs->setStatus(QuestStatus::REWARD);
						qs->setRewardGroup(0);
						updateQuestStatus(env);
						removeQuestItem(env, 182200217, 1);
						return sendQuestDialog(env, 5);
					}
					return false;
				case SETPRO2:
					if (var == 3) {
						PacketSendUtility::sendPacket(*player, SM_DIALOG_WINDOW(env.getVisibleObject()->getObjectId(), 10));
						return true;
					}
			}
		}
		return false;
	}

	HandlerResult onItemUseEvent(QuestEnv& env, Item& item) override {
		runtime::Ptr<Player> player = env.getPlayer();
		int32_t id = item.getItemTemplate()->getTemplateId();
		runtime::Ptr<QuestState> qs = player->getQuestStateList()->getQuestState(questId);

		if (id != 182200214)
			return HandlerResult::UNKNOWN;

		if (qs == nullptr || qs->isStartable()) {
			QuestService::startQuest(env);
		}
		return HandlerResult::SUCCESS;
	}
};
AION_QUEST_HANDLER(_1114TheNymphsGown, 1114);

} // namespace aion::gameserver::handlers::quest::poeta
