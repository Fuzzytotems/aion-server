#include "aion/gameserver/handlers/quest/QuestPrelude.h"

#include <array>

#include "aion/gameserver/configs/main/CustomConfig.h"
#include "aion/gameserver/model/PlayerClassInfo.h"
#include "aion/gameserver/model/gameobjects/player/QuestStateList.h"
#include "aion/gameserver/model/templates/quest/QuestNpc.h"
#include "aion/gameserver/network/aion/serverpackets/SM_DIALOG_WINDOW.h"
#include "aion/gameserver/questEngine/model/QuestVars.h"
#include "aion/gameserver/services/teleport/TeleportService.h"

namespace aion::gameserver::handlers::quest::ascension {

// Hand-ported (lane P6-Q asc-hand, chunk Q06) from game-server/data/handlers/quest/ascension/_1007ACeremonyinSanctum.java, statement by
// statement in questgen's conventions; questgen refuses the file (the config flag). docs/deviations/Q06.md.

/**
 * @author MrPoke + Dune11, vlog
 */
class _1007ACeremonyinSanctum final : public AbstractQuestHandler {
public:
	_1007ACeremonyinSanctum() : AbstractQuestHandler(1007) {}

	void register_() override {
		if (CustomConfig::ENABLE_SIMPLE_2NDCLASS)
			return;
		std::array<int32_t, 9> npcs{790001, 203725, 203752, 203758, 203759, 203760, 203761, 801212, 801213};
		qe.registerOnLevelChanged(questId);
		qe.registerOnQuestCompleted(questId);
		for (int32_t npc : npcs) {
			qe.registerQuestNpc(npc)->addOnTalkEvent(questId);
		}
	}

	bool onDialogEvent(QuestEnv& env) override {
		runtime::Ptr<Player> player = env.getPlayer();
		runtime::Ptr<QuestState> qs = player->getQuestStateList()->getQuestState(questId);
		int32_t dialogActionId = env.getDialogActionId();
		int32_t targetId = env.getTargetId();
		if (qs == nullptr) {
			return false;
		}
		int32_t var = qs->getQuestVars()->getQuestVars();

		if (qs->getStatus() == QuestStatus::START) {
			switch (targetId) {
				case 790001: // Pernos
					switch (dialogActionId) {
						case QUEST_SELECT:
							if (var == 0) {
								return sendQuestDialog(env, 1011);
							}
							if (var == 1) {
								return sendQuestDialog(env, 1013);
							}
							return false;
						case SETPRO1:
							if (var <= 1) {
								qs->setQuestVar(1);
								updateQuestStatus(env);
								PacketSendUtility::sendPacket(*player, SM_DIALOG_WINDOW(env.getVisibleObject()->getObjectId(), 0));
								TeleportService::teleportTo(*player, 110010000, 1313.0f, 1512.0f, 568.0f, static_cast<int8_t>(0), TeleportAnimation::FADE_OUT_BEAM);
								return true;
							}
					}
					break;
				case 203725: // Leah
					switch (dialogActionId) {
						case QUEST_SELECT:
							if (var == 1) {
								return sendQuestDialog(env, 1352);
							}
							return false;
						case SELECT2_1:
							return playQuestMovie(env, 92);
						case SETPRO2:
							return defaultCloseDialog(env, 1, 2); // 2
					}
					break;
				case 203752: // Jucleas
					switch (dialogActionId) {
						case QUEST_SELECT:
							if (var == 2) {
								return sendQuestDialog(env, 1693);
							}
							return false;
						case SELECT3_1:
							return playQuestMovie(env, 91);
						case SETPRO3:
							if (var == 2) {
								switch (::aion::gameserver::model::getStartingClass(player->getPlayerClass())) {
									case PlayerClass::WARRIOR:
										qs->setQuestVar(10);
										qs->setRewardGroup(0);
										break;
									case PlayerClass::SCOUT:
										qs->setQuestVar(20);
										qs->setRewardGroup(1);
										break;
									case PlayerClass::MAGE:
										qs->setQuestVar(30);
										qs->setRewardGroup(2);
										break;
									case PlayerClass::PRIEST:
										qs->setQuestVar(40);
										qs->setRewardGroup(3);
										break;
									case PlayerClass::ENGINEER:
										qs->setQuestVar(50);
										qs->setRewardGroup(4);
										break;
									case PlayerClass::ARTIST:
										qs->setQuestVar(60);
										qs->setRewardGroup(5);
										break;
								}
								qs->setStatus(QuestStatus::REWARD);
								updateQuestStatus(env);
								return sendQuestSelectionDialog(env);
							}
							break;
					}
					break;
			}
		} else if (qs->getStatus() == QuestStatus::REWARD) {
			if (targetId == 203758 && var == 10) {
				switch (env.getDialogActionId()) {
					case USE_OBJECT:
						return sendQuestDialog(env, 2034);
					default:
						return sendQuestEndDialog(env);
				}
			} else if (targetId == 203759 && var == 20) {
				switch (env.getDialogActionId()) {
					case USE_OBJECT:
						return sendQuestDialog(env, 2375);
					default:
						return sendQuestEndDialog(env);
				}
			} else if (targetId == 203760 && var == 30) {
				switch (env.getDialogActionId()) {
					case USE_OBJECT:
						return sendQuestDialog(env, 2716);
					default:
						return sendQuestEndDialog(env);
				}
			} else if (targetId == 203761 && var == 40) {
				switch (env.getDialogActionId()) {
					case USE_OBJECT:
						return sendQuestDialog(env, 3057);
					default:
						return sendQuestEndDialog(env);
				}
			} else if (targetId == 801212 && var == 50) {
				switch (env.getDialogActionId()) {
					case USE_OBJECT:
						return sendQuestDialog(env, 3398);
					default:
						return sendQuestEndDialog(env);
				}
			} else if (targetId == 801213 && var == 60) {
				switch (env.getDialogActionId()) {
					case USE_OBJECT:
						return sendQuestDialog(env, 3739);
					default:
						return sendQuestEndDialog(env);
				}
			}
		}
		return false;
	}

	void onLevelChangedEvent(Player& player) override {
		defaultOnLevelChangedEvent(player, {1006});
	}

	void onQuestCompletedEvent(QuestEnv& env) override {
		defaultOnQuestCompletedEvent(env, {1006});
	}
};
AION_QUEST_HANDLER(_1007ACeremonyinSanctum, 1007);

} // namespace aion::gameserver::handlers::quest::ascension
