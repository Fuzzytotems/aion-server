#include "aion/gameserver/handlers/quest/QuestPrelude.h"

#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/controllers/VisibleObjectController.h"
#include "aion/gameserver/model/gameobjects/player/QuestStateList.h"
#include "aion/gameserver/model/templates/quest/QuestNpc.h"
#include "aion/gameserver/network/aion/serverpackets/SM_DIALOG_WINDOW.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"
#include "aion/gameserver/world/WorldMapInstance.h"

namespace aion::gameserver::handlers::quest::heiron {

// Hand-ported from game-server/data/handlers/quest/heiron/_1643TheStarOfHeiron.java (questgen refuses it: the anonymous Runnable of :98),
// statement by statement in the questgen output's style (docs/deviations/Q03.md, Hand ports).

/**
 * @author Balthazar
 */
class _1643TheStarOfHeiron final : public AbstractQuestHandler {
public:
	_1643TheStarOfHeiron() : AbstractQuestHandler(1643) {}

	void register_() override {
		qe.registerOnEnterWorld(questId);
		qe.registerQuestNpc(204545)->addOnQuestStart(questId);
		qe.registerQuestNpc(204545)->addOnTalkEvent(questId);
		qe.registerQuestNpc(204630)->addOnTalkEvent(questId);
		qe.registerQuestNpc(204614)->addOnTalkEvent(questId);
	}

	bool onDialogEvent(QuestEnv& env) override {
		runtime::Ptr<Player> player = env.getPlayer();
		runtime::Ptr<QuestState> qs = player->getQuestStateList()->getQuestState(questId);

		int32_t targetId = 0;
		if (runtime::as<Npc>(env.getVisibleObject()) != nullptr)
			targetId = (runtime::cast<Npc>(env.getVisibleObject()))->getNpcId();

		if (qs == nullptr || qs->isStartable()) {
			if (targetId == 204545) {
				switch (env.getDialogActionId()) {
					case QUEST_SELECT:
						return sendQuestDialog(env, 4762);
					case ASK_QUEST_ACCEPT:
					case QUEST_ACCEPT_1:
						return sendQuestStartDialog(env, 182201764, 1);
				}
			}
		}

		if (qs == nullptr)
			return false;

		if (qs->getStatus() == QuestStatus::START) {
			switch (targetId) {
				case 204630:
					switch (env.getDialogActionId()) {
						case QUEST_SELECT:
							{
								if (qs->getQuestVarById(0) == 0) {
									return sendQuestDialog(env, 1011);
								} else if (qs->getQuestVarById(0) == 2) {
									return sendQuestDialog(env, 1693);
								}
								return false;
							}
						case SETPRO1: // java-bug kept: no var guard, each SETPRO1 at Erato adds 1 and spawns another spirit (docs/deviations/Q03.md)
							{
								qs->setQuestVarById(0, qs->getQuestVarById(0) + 1);
								removeQuestItem(env, 182201764, 1);
								updateQuestStatus(env);
								PacketSendUtility::sendPacket(*player, SM_DIALOG_WINDOW(env.getVisibleObject()->getObjectId(), 0));
								spawnForFiveMinutes(204614, *player->getWorldMapInstance(), static_cast<float>(1591.4327), static_cast<float>(2774.2283), static_cast<float>(127.63001), static_cast<int8_t>(0));
								return true;
							}
						case SET_SUCCEED: // java-bug kept: no var guard, SET_SUCCEED at any START var sets REWARD (docs/deviations/Q03.md)
							{
								qs->setStatus(QuestStatus::REWARD);
								updateQuestStatus(env);
								PacketSendUtility::sendPacket(*player, SM_DIALOG_WINDOW(env.getVisibleObject()->getObjectId(), 0));
								return true;
							}
					}
					return false;
				case 204614:
					switch (env.getDialogActionId()) {
						case QUEST_SELECT:
							{
								if (qs->getQuestVarById(0) == 1) {
									return sendQuestDialog(env, 1011);
								}
								return false;
							}
						case SETPRO1: // java-bug kept: no var guard, SETPRO1 at the spirit adds 1 at any START var (docs/deviations/Q03.md)
							{
								qs->setQuestVarById(0, qs->getQuestVarById(0) + 1);
								updateQuestStatus(env);
								PacketSendUtility::sendPacket(*player, SM_DIALOG_WINDOW(env.getVisibleObject()->getObjectId(), 10));
								runtime::Ptr<Npc> npc = runtime::cast<Npc>(env.getVisibleObject());
								// anonymous Runnable at _1643TheStarOfHeiron.java:98 (fieldmap _1643TheStarOfHeiron$1, storage: task): its one capture, the
								// npc (never null here: targetId is an npc id only when the visible object is an Npc), is the Pin of the task
								Npc& star = *npc;
								ThreadPoolManager::getInstance().schedule({&star}, [&star] { star.getController().delete_(); }, 40000);
								return true;
							}
					}
			}
		} else if (qs->getStatus() == QuestStatus::REWARD) {
			if (targetId == 204545) {
				if (env.getDialogActionId() == SELECT_QUEST_REWARD)
					return sendQuestDialog(env, 5);
				else
					return sendQuestEndDialog(env);
			}
		}
		return false;
	}

	bool onEnterWorldEvent(QuestEnv& env) override {
		runtime::Ptr<Player> player = env.getPlayer();
		runtime::Ptr<QuestState> qs = player->getQuestStateList()->getQuestState(questId);

		if (qs == nullptr) {
			return false;
		}

		if (qs->getStatus() == QuestStatus::START) {
			if (qs->getQuestVarById(0) == 1) {
				qs->setQuestVar(0);
				updateQuestStatus(env);
			}
		}
		return false;
	}
};
AION_QUEST_HANDLER(_1643TheStarOfHeiron, 1643);

} // namespace aion::gameserver::handlers::quest::heiron
