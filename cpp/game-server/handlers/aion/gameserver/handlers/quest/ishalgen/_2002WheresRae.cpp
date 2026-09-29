#include "aion/gameserver/handlers/quest/QuestPrelude.h"

#include <array>

#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/controllers/VisibleObjectController.h"
#include "aion/gameserver/model/gameobjects/player/QuestStateList.h"
#include "aion/gameserver/model/templates/quest/QuestNpc.h"
#include "aion/gameserver/network/aion/serverpackets/SM_DIALOG_WINDOW.h"
#include "aion/gameserver/network/aion/serverpackets/SM_EMOTION.h"
#include "aion/gameserver/services/QuestService.h"
#include "aion/gameserver/services/instance/InstanceService.h"
#include "aion/gameserver/services/teleport/TeleportService.h"
#include "aion/gameserver/skillengine/SkillEngine.h"
#include "aion/gameserver/skillengine/model/Effect.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldMapTypeInfo.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::handlers::quest::ishalgen {

// Hand-ported from game-server/data/handlers/quest/ishalgen/_2002WheresRae.java (questgen refuses it: the anonymous Runnable of :182),
// statement by statement in the questgen output's style (docs/deviations/Q09-hand.md).

/**
 * @author Mr. Poke, Hellboy, Gigi, Bobobear, Majka
 */
class _2002WheresRae final : public AbstractQuestHandler {
public:
	_2002WheresRae() : AbstractQuestHandler(2002) {}

	void register_() override {
		std::array<int32_t, 6> npc_ids{203519, 203534, 203553, 700045, 203516, 203538};
		qe.registerOnQuestCompleted(questId);
		qe.registerOnLevelChanged(questId);
		qe.registerQuestNpc(210377)->addOnKillEvent(questId);
		qe.registerQuestNpc(210378)->addOnKillEvent(questId);
		for (int32_t npc_id : npc_ids)
			qe.registerQuestNpc(npc_id)->addOnTalkEvent(questId);
	}

	bool onDialogEvent(QuestEnv& env) override {
		runtime::Ptr<Player> player = env.getPlayer();
		runtime::Ptr<QuestState> qs = player->getQuestStateList()->getQuestState(questId);
		QuestEnv& qEnv = env;
		if (qs == nullptr)
			return false;

		int32_t var = qs->getQuestVarById(0);
		int32_t targetId = env.getTargetId();
		int32_t dialogActionId = env.getDialogActionId();
		if (qs->getStatus() == QuestStatus::START) {
			switch (targetId) {
				case 203519:
					switch (dialogActionId) {
						case QUEST_SELECT:
							if (var == 0)
								return sendQuestDialog(env, 1011);
							return false;
						case SETPRO1:
							if (var == 0) {
								qs->setQuestVarById(0, var + 1);
								updateQuestStatus(env);
								PacketSendUtility::sendPacket(*player, SM_DIALOG_WINDOW(env.getVisibleObject()->getObjectId(), 10));
								return true;
							}
					}
					return false;
				case 203534:
					switch (dialogActionId) {
						case QUEST_SELECT:
							if (var == 1)
								return sendQuestDialog(env, 1352);
							return false;
						case SELECT2_1:
							playQuestMovie(env, 52);
							break;
						case SETPRO2:
							if (var == 1) {
								qs->setQuestVarById(0, var + 1);
								updateQuestStatus(env);
								PacketSendUtility::sendPacket(*player, SM_DIALOG_WINDOW(env.getVisibleObject()->getObjectId(), 10));
								return true;
							}
					}
					return false;
				case 790002:
					switch (dialogActionId) {
						case QUEST_SELECT:
							if (var == 2)
								return sendQuestDialog(env, 1693);
							else if (var == 10)
								return sendQuestDialog(env, 2034);
							else if (var == 11)
								return sendQuestDialog(env, 2375);
							else if (var == 12)
								return sendQuestDialog(env, 2462);
							else if (var == 13)
								return sendQuestDialog(env, 2716);
							return false;
						case SETPRO3:
						case SETPRO4:
						case SETPRO6:
							if (var == 2 || var == 10) {
								qs->setQuestVarById(0, var + 1);
								updateQuestStatus(env);
								PacketSendUtility::sendPacket(*player, SM_DIALOG_WINDOW(env.getVisibleObject()->getObjectId(), 10));
								return true;
							} else if (var == 13) {
								qs->setQuestVarById(0, 14);
								updateQuestStatus(env);
								PacketSendUtility::sendPacket(*player, SM_DIALOG_WINDOW(env.getVisibleObject()->getObjectId(), 10));
								return true;
							}
							break;
						case SETPRO5:
							// java-bug kept: getQuestVarById(0) is one 6-bit slot (QuestVars.java:23-58), so var == 99 never holds; the setQuestVar(99)
							// below leaves slot 0 at 35 (99 & 0x3F), and a player who leaves Ataxiar before talking to Hagen (205020) inside cannot
							// enter it again through Rae (docs/deviations/Q09-hand.md)
							if (var == 12 || var == 99) {
								qs->setQuestVar(99);
								updateQuestStatus(env);
								PacketSendUtility::sendPacket(*player, SM_DIALOG_WINDOW(env.getVisibleObject()->getObjectId(), 0));
								// Create instance
								runtime::Ptr<WorldMapInstance> newInstance =
									InstanceService::getNextAvailableInstance(::aion::gameserver::world::getId(WorldMapType::ATAXIAR), *player);
								TeleportService::teleportTo(*player, *newInstance, 457.65f, 426.8f, 230.4f);
								return true;
							}
							return false;
						case CHECK_USER_HAS_QUEST_ITEM:
							if (var == 11) {
								if (QuestService::collectItemCheck(env, true)) {
									qs->setQuestVarById(0, 12);
									updateQuestStatus(env);
									return sendQuestDialog(env, 2461);
								} else
									return sendQuestDialog(env, 2376);
							}
					}
					break;
				case 700045:
					if (var == 11 && env.getDialogActionId() == USE_OBJECT) {
						SkillEngine::getInstance().applyEffectDirectly(8343, *player, *player);
						return true;
					}
					return false;
				case 203538:
					if (var == 14 && env.getDialogActionId() == USE_OBJECT) {
						qs->setQuestVarById(0, var + 1);
						updateQuestStatus(env);
						runtime::Ptr<Npc> npc = runtime::cast<Npc>(env.getVisibleObject());
						PacketSendUtility::sendPacket(*player, SM_DIALOG_WINDOW(npc->getObjectId(), 10));
						spawnForFiveMinutes(203553, *npc->getPosition());
						npc->getController().deleteAndScheduleRespawn();
						return true;
					}
					break;
				case 203553:
					switch (dialogActionId) {
						case QUEST_SELECT:
							if (var == 15)
								return sendQuestDialog(env, 3057);
							return false;
						case SETPRO7:
							if (var == 15) {
								env.getVisibleObject()->getController().delete_();
								qs->setStatus(QuestStatus::REWARD);
								updateQuestStatus(env);
								PacketSendUtility::sendPacket(*player, SM_DIALOG_WINDOW(env.getVisibleObject()->getObjectId(), 10));
								return true;
							}
					}
					return false;
				case 205020:
					switch (dialogActionId) {
						case QUEST_SELECT:
							if (var >= 12) {
								player->setState(CreatureState::FLYING);
								player->unsetState(CreatureState::ACTIVE);
								player->setFlightTeleportId(3001);
								PacketSendUtility::sendPacket(*player, SM_EMOTION(*player, EmotionType::START_FLYTELEPORT, 3001, 0));
								// anonymous Runnable at _2002WheresRae.java:182 (fieldmap _2002WheresRae$1, storage: task): its captures - this handler
								// (Immortal, RT-11), the player, the quest state and qEnv - are the Pin of the task
								Player& flyer = *player;
								QuestState& state = *qs;
								ThreadPoolManager::getInstance().schedule({this, &flyer, &state, &qEnv},
									[this, &flyer, &state, &qEnv] {
										TeleportService::teleportTo(flyer, 220010000, 940.15f, 2295.64f, 265.7f, static_cast<int8_t>(43));
										state.setQuestVar(13);
										updateQuestStatus(qEnv);
									},
									40000);
								return true;
							}
							return false;
						default:
							return false;
					}
			}
		} else if (qs->getStatus() == QuestStatus::REWARD) {
			if (targetId == 203516) {
				if (dialogActionId == USE_OBJECT || dialogActionId == QUEST_SELECT)
					return sendQuestDialog(env, 3398);
				else if (dialogActionId == SETPRO8)
					return sendQuestDialog(env, 5);
				else
					return sendQuestEndDialog(env);
			}
		}
		return false;
	}

	bool onKillEvent(QuestEnv& env) override {
		runtime::Ptr<Player> player = env.getPlayer();
		runtime::Ptr<QuestState> qs = player->getQuestStateList()->getQuestState(questId);
		if (qs == nullptr)
			return false;

		int32_t var = qs->getQuestVarById(0);
		int32_t targetId = 0;
		if (runtime::as<Npc>(env.getVisibleObject()) != nullptr)
			targetId = (runtime::cast<Npc>(env.getVisibleObject()))->getNpcId();

		if (qs->getStatus() != QuestStatus::START)
			return false;
		switch (targetId) {
			case 210377:
			case 210378:
				if (var >= 3 && var < 10) {
					qs->setQuestVarById(0, qs->getQuestVarById(0) + 1);
					updateQuestStatus(env);
					return true;
				}
		}
		return false;
	}

	void onQuestCompletedEvent(QuestEnv& env) override {
		defaultOnQuestCompletedEvent(env, {2100});
	}

	void onLevelChangedEvent(Player& player) override {
		defaultOnLevelChangedEvent(player, {2100});
	}
};
AION_QUEST_HANDLER(_2002WheresRae, 2002);

} // namespace aion::gameserver::handlers::quest::ishalgen
