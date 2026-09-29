#include "aion/gameserver/handlers/quest/QuestPrelude.h"

#include <any>
#include <array>
#include <span>

#include "aion/gameserver/controllers/VisibleObjectController.h"
#include "aion/gameserver/model/gameobjects/player/QuestStateList.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/model/templates/quest/QuestNpc.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ASCENSION_MORPH.h"
#include "aion/gameserver/network/aion/serverpackets/SM_EMOTION.h"
#include "aion/gameserver/services/instance/InstanceService.h"
#include "aion/gameserver/services/teleport/TeleportService.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldMapTypeInfo.h"

namespace aion::gameserver::handlers::quest::poeta {

// Hand-ported from game-server/data/handlers/quest/poeta/_1002RequestoftheElim.java (questgen refuses it: the anonymous Runnable of :140 and
// new SM_ASCENSION_MORPH of :170), statement by statement in the questgen output's style (docs/deviations/Q05-hand.md).

/**
 * @author MrPoke, vlog, Majka
 */
class _1002RequestoftheElim final : public AbstractQuestHandler {
public:
	_1002RequestoftheElim() : AbstractQuestHandler(1002) {}

	void register_() override {
		std::array<int32_t, 6> npcs{203076, 730007, 730010, 730008, 205000, 203067};
		qe.registerOnQuestCompleted(questId);
		qe.registerOnLevelChanged(questId);
		qe.registerOnEnterWorld(questId);
		for (int32_t npc : npcs) {
			qe.registerQuestNpc(npc)->addOnTalkEvent(questId);
		}
	}

	bool onDialogEvent(QuestEnv& env) override {
		runtime::Ptr<Player> player = env.getPlayer();
		runtime::Ptr<QuestState> qs = player->getQuestStateList()->getQuestState(questId);
		int32_t dialogActionId = env.getDialogActionId();
		if (qs == nullptr)
			return false;
		int32_t var = qs->getQuestVarById(0);
		int32_t targetId = env.getTargetId();

		if (qs->getStatus() == QuestStatus::START) {
			switch (targetId) {
				case 203076: // Ampeis
					switch (dialogActionId) {
						case QUEST_SELECT:
							if (var == 0) {
								return sendQuestDialog(env, 1011);
							}
							return false;
						case SETPRO1:
							return defaultCloseDialog(env, 0, 1); // 1
					}
					break;
				case 730007: // Forest Protector Noah
					switch (dialogActionId) {
						case QUEST_SELECT:
							if (var == 1) {
								return sendQuestDialog(env, 1352);
							} else if (var == 5) {
								return sendQuestDialog(env, 1693);
							} else if (var == 6) {
								return sendQuestDialog(env, 2034);
							} else if (var == 12) {
								return sendQuestDialog(env, 2120);
							}
							return false;
						case SELECT2_1:
							if (var == 1) {
								playQuestMovie(env, 20);
								return sendQuestDialog(env, 1353);
							}
							return false;
						case SETPRO2:
							return defaultCloseDialog(env, 1, 2, 182200002, 1, 0, 0); // 2
						case SETPRO3:
							return defaultCloseDialog(env, 5, 6, 0, 0, 182200002, 1); // 6
						case CHECK_USER_HAS_QUEST_ITEM:
							if (var == 6) {
								return checkQuestItems(env, 6, 12, false, 2120, 2205); // 12
							} else if (var == 12) {
								return sendQuestDialog(env, 2120);
							}
							return false;
						case SETPRO4:
							return defaultCloseDialog(env, 12, 13); // 13
						case FINISH_DIALOG:
							return sendQuestSelectionDialog(env);
					}
					break;
				case 730010: // Sleeping Elder
					if (dialogActionId == USE_OBJECT) {
						if (player->getInventory().getItemCountByItemId(182200002) == 1) {
							if (var == 2) {
								env.getVisibleObject()->getController().deleteAndScheduleRespawn();
								return useQuestObject(env, 2, 4, false, false); // 4
							} else if (var == 4) {
								env.getVisibleObject()->getController().deleteAndScheduleRespawn();
								return useQuestObject(env, 4, 5, false, false); // 5
							}
						}
					}
					break;
				case 730008: // Daminu
					switch (dialogActionId) {
						case QUEST_SELECT:
							if (var == 13) {
								return sendQuestDialog(env, 2375);
							} else if (var == 14) {
								return sendQuestDialog(env, 2461);
							}
							return false;
						case SETPRO5: {
							runtime::Ptr<WorldMapInstance> newInstance =
								InstanceService::getNextAvailableInstance(::aion::gameserver::world::getId(WorldMapType::KARAMATIS), *player);
							TeleportService::teleportTo(*player, *newInstance, 52, 174, 229);
							changeQuestStep(env, 13, 20); // 20
							return closeDialogWindow(env);
						}
						case SETPRO6:
							return defaultCloseDialog(env, 14, 14, true, false); // reward
					}
					break;
				case 205000: // Belpartan
					switch (dialogActionId) {
						case QUEST_SELECT:
							if (var == 20) {
								player->setState(CreatureState::FLYING);
								player->unsetState(CreatureState::ACTIVE);
								player->setFlightTeleportId(1001);
								PacketSendUtility::sendPacket(*player, SM_EMOTION(*player, EmotionType::START_FLYTELEPORT, 1001, 0));
								// anonymous Runnable at _1002RequestoftheElim.java:140 (fieldmap _1002RequestoftheElim$1, storage: task): its captures -
								// this handler (Immortal, RT-11), the env and the player - are the Pin of the task, as AbstractQuestHandler.cpp:955 does
								Player& flyer = *player;
								ThreadPoolManager::getInstance().schedule({this, &env, &flyer},
									[this, &env, &flyer] {
										changeQuestStep(env, 20, 14); // 14
										TeleportService::teleportTo(flyer, 210010000, 1, 603, 1537, 116, static_cast<int8_t>(20));
									},
									43000);
								return true;
							}
					}
			}
		} else if (qs->getStatus() == QuestStatus::REWARD) {
			if (targetId == 203067) { // Kalio
				if (dialogActionId == USE_OBJECT) {
					return sendQuestDialog(env, 2716);
				} else {
					return sendQuestEndDialog(env);
				}
			}
		}
		return false;
	}

	bool onEnterWorldEvent(QuestEnv& env) override {
		runtime::Ptr<Player> player = env.getPlayer();
		runtime::Ptr<QuestState> qs = player->getQuestStateList()->getQuestState(questId);
		if (qs != nullptr && qs->getStatus() == QuestStatus::START) {
			if (player->getWorldId() == 310010000) {
				PacketSendUtility::sendPacket(*player, SM_ASCENSION_MORPH(1));
				return true;
			} else {
				int32_t var = qs->getQuestVarById(0);
				if (var == 20) {
					changeQuestStep(env, 20, 13); // 13
				}
			}
		}
		return false;
	}

	bool onCanAct(QuestEnv& env, QuestActionType questEventType, std::span<const std::any> objects) override {
		runtime::Ptr<Player> player = env.getPlayer();
		runtime::Ptr<QuestState> qs = player->getQuestStateList()->getQuestState(env.getQuestId());
		int32_t targetId = env.getTargetId();
		if (targetId == 730010) {
			if (qs == nullptr || qs->getStatus() != QuestStatus::START || (qs->getQuestVarById(0) != 2 && qs->getQuestVarById(0) != 4)) {
				return false;
			}
		}
		return true;
	}

	void onQuestCompletedEvent(QuestEnv& env) override {
		defaultOnQuestCompletedEvent(env, {1100});
	}

	void onLevelChangedEvent(Player& player) override {
		defaultOnLevelChangedEvent(player, {1100});
	}
};
AION_QUEST_HANDLER(_1002RequestoftheElim, 1002);

} // namespace aion::gameserver::handlers::quest::poeta
