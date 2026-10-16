#include "aion/gameserver/handlers/quest/QuestPrelude.h"

#include "aion/gameserver/model/gameobjects/player/QuestStateList.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/model/templates/quest/QuestNpc.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ITEM_USAGE_ANIMATION.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"

namespace aion::gameserver::handlers::quest::altgard {

// Hand-ported from game-server/data/handlers/quest/altgard/_2208MauInTenMinutesADay.java (questgen refuses it: the anonymous Runnable of
// :85), statement by statement in the questgen output's style (docs/deviations/Q10.md, Hand ports).

/**
 * @author Mr. Poke
 */
class _2208MauInTenMinutesADay final : public AbstractQuestHandler {
public:
	_2208MauInTenMinutesADay() : AbstractQuestHandler(2208) {}

	void register_() override {
		qe.registerQuestNpc(203591)->addOnQuestStart(questId);
		qe.registerQuestNpc(203591)->addOnTalkEvent(questId);
		qe.registerQuestNpc(203589)->addOnTalkEvent(questId);
		qe.registerQuestItem(182203205, questId);
	}

	bool onDialogEvent(QuestEnv& env) override {
		runtime::Ptr<Player> player = env.getPlayer();
		int32_t targetId = 0;
		if (runtime::as<Npc>(env.getVisibleObject()) != nullptr)
			targetId = (runtime::cast<Npc>(env.getVisibleObject()))->getNpcId();
		runtime::Ptr<QuestState> qs = player->getQuestStateList()->getQuestState(questId);
		if (qs == nullptr || qs->isStartable()) {
			if (targetId == 203591) {
				if (env.getDialogActionId() == QUEST_SELECT)
					return sendQuestDialog(env, 1011);
				else if (env.getDialogActionId() == QUEST_ACCEPT_1) {
					if (giveQuestItem(env, 182203205, 1))
						return sendQuestStartDialog(env);
					return true;
				} else
					return sendQuestStartDialog(env);
			}
		} else if (qs->getStatus() == QuestStatus::START) {
			if (targetId == 203589) {
				int32_t var = qs->getQuestVarById(0);
				if (env.getDialogActionId() == QUEST_SELECT) {
					if (var == 0)
						return sendQuestDialog(env, 1693);
					else if (var == 1)
						return sendQuestDialog(env, 1352);
				} else if (env.getDialogActionId() == SETPRO1) {
					qs->setStatus(QuestStatus::REWARD);
					updateQuestStatus(env);
					return sendQuestSelectionDialog(env);
				}
			}
		} else if (qs->getStatus() == QuestStatus::REWARD) {
			if (targetId == 203591)
				return sendQuestEndDialog(env);
		}
		return false;
	}

	HandlerResult onItemUseEvent(QuestEnv& env, Item& item) override {
		runtime::Ptr<Player> player = env.getPlayer();
		int32_t id = item.getItemTemplate()->getTemplateId();
		int32_t itemObjId = item.getObjectId();

		if (id != 182203205)
			return HandlerResult::UNKNOWN;
		runtime::Ptr<QuestState> qs = player->getQuestStateList()->getQuestState(questId);
		if (qs == nullptr)
			return HandlerResult::FAILED;
		PacketSendUtility::broadcastPacket(*player, SM_ITEM_USAGE_ANIMATION(player->getObjectId(), itemObjId, id, 3000, 0, 0), true);
		// anonymous Runnable at _2208MauInTenMinutesADay.java:85 (fieldmap _2208MauInTenMinutesADay$1, storage: task): its captures - this
		// handler (Immortal, RT-11), the player, the quest state and the env - are the Pin of the task; id and itemObjId are copied
		Player& user = *player;
		QuestState& state = *qs;
		ThreadPoolManager::getInstance().schedule({this, &user, &state, &env},
			[this, &user, &state, &env, itemObjId, id] {
				PacketSendUtility::broadcastPacket(user, SM_ITEM_USAGE_ANIMATION(user.getObjectId(), itemObjId, id, 0, 1, 0), true);
				user.getInventory().decreaseByObjectId(itemObjId, 1);
				state.setQuestVarById(0, 1);
				updateQuestStatus(env);
			},
			3000);
		return HandlerResult::SUCCESS;
	}
};
AION_QUEST_HANDLER(_2208MauInTenMinutesADay, 2208);

} // namespace aion::gameserver::handlers::quest::altgard
