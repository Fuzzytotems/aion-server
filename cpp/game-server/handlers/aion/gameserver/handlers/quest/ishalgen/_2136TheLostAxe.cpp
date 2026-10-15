#include "aion/gameserver/handlers/quest/QuestPrelude.h"

#include <array>

#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/model/gameobjects/player/QuestStateList.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/model/templates/quest/QuestNpc.h"
#include "aion/gameserver/network/aion/serverpackets/SM_DIALOG_WINDOW.h"
#include "aion/gameserver/services/QuestService.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"
#include "aion/gameserver/world/WorldMapInstance.h"

namespace aion::gameserver::handlers::quest::ishalgen {

// Hand-ported from game-server/data/handlers/quest/ishalgen/_2136TheLostAxe.java (questgen refuses it: the anonymous Runnable of :61),
// statement by statement in the questgen output's style (docs/deviations/Q09.md, Hand ports).

/**
 * @author Rhys2002, Hellboy
 */
class _2136TheLostAxe final : public AbstractQuestHandler {
private:
	// fieldmap: a table of literals that is never written (Java's effectively final int[]), questgen's static constexpr std::array
	static constexpr std::array<int32_t, 2> npc_ids{700146, 790009};

public:
	_2136TheLostAxe() : AbstractQuestHandler(2136) {}

	void register_() override {
		qe.registerQuestItem(182203130, questId);
		for (int32_t npc_id : npc_ids)
			qe.registerQuestNpc(npc_id)->addOnTalkEvent(questId);
	}

	bool onDialogEvent(QuestEnv& env) override {
		runtime::Ptr<Player> player = env.getPlayer();
		int32_t targetId = 0;
		runtime::Ptr<QuestState> qs = player->getQuestStateList()->getQuestState(questId);
		if (runtime::as<Npc>(env.getVisibleObject()) != nullptr)
			targetId = (runtime::cast<Npc>(env.getVisibleObject()))->getNpcId();

		if (qs == nullptr || qs->isStartable()) {
			if (env.getDialogActionId() == QUEST_ACCEPT_1) {
				QuestService::startQuest(env);
				PacketSendUtility::sendPacket(*player, SM_DIALOG_WINDOW(0, 0));
				return true;
			} else
				PacketSendUtility::sendPacket(*player, SM_DIALOG_WINDOW(0, 0));
		}

		if (qs == nullptr)
			return false;

		int32_t var = qs->getQuestVarById(0);

		if (qs->getStatus() == QuestStatus::REWARD) {
			if (targetId == 790009) {
				runtime::Ptr<Npc> npc = runtime::cast<Npc>(env.getVisibleObject());
				// anonymous Runnable at _2136TheLostAxe.java:61 (fieldmap _2136TheLostAxe$1, storage: task): its one capture, the npc (never null
				// here: targetId is an npc id only when the visible object is an Npc), is the Pin of the task
				Npc& axeOwner = *npc;
				ThreadPoolManager::getInstance().schedule({&axeOwner}, [&axeOwner] { axeOwner.getController().delete_(); }, 10000);
				return sendQuestEndDialog(env);
			}
		} else if (qs->getStatus() != QuestStatus::START)
			return false;

		if (targetId == 790009) {
			switch (env.getDialogActionId()) {
				case QUEST_SELECT:
					if (var == 1)
						return sendQuestDialog(env, 1011);
					return false;
				case SETPRO1:
					if (var == 1) {
						qs->setStatus(QuestStatus::REWARD);
						updateQuestStatus(env);
						removeQuestItem(env, 182203130, 1);
						qs->setRewardGroup(1);
						return sendQuestDialog(env, 6);
					}
					return false;
				case SETPRO2:
					if (var == 1) {
						qs->setStatus(QuestStatus::REWARD);
						updateQuestStatus(env);
						removeQuestItem(env, 182203130, 1);
						qs->setRewardGroup(0);
						return sendQuestDialog(env, 5);
					}
			}
		} else if (targetId == 700146) {
			switch (env.getDialogActionId()) {
				case USE_OBJECT:
					if (var == 0) {
						playQuestMovie(env, 59);
						qs->setQuestVarById(0, 1);
						updateQuestStatus(env);
						spawnForFiveMinutes(790009, *player->getWorldMapInstance(), 1088.5f, 2371.8f, 258.375f, static_cast<int8_t>(87));
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

		if (id != 182203130)
			return HandlerResult::UNKNOWN;
		if (qs == nullptr || qs->isStartable()) {
			QuestService::startQuest(env);
		}
		return HandlerResult::SUCCESS;
	}
};
AION_QUEST_HANDLER(_2136TheLostAxe, 2136);

} // namespace aion::gameserver::handlers::quest::ishalgen
