#include "aion/gameserver/handlers/quest/QuestPrelude.h"

#include "aion/gameserver/configs/main/CustomConfig.h"
#include "aion/gameserver/model/PlayerClassInfo.h"
#include "aion/gameserver/model/gameobjects/player/QuestStateList.h"
#include "aion/gameserver/model/templates/quest/QuestNpc.h"
#include "aion/gameserver/network/aion/serverpackets/SM_DIALOG_WINDOW.h"
#include "aion/gameserver/questEngine/model/QuestVars.h"
#include "aion/gameserver/services/teleport/TeleportService.h"

namespace aion::gameserver::handlers::quest::ascension {

// Hand-ported (lane P6-Q asc-hand, chunk Q06) from game-server/data/handlers/quest/ascension/_2009ACeremonyinPandaemonium.java, statement by
// statement in questgen's conventions; questgen refuses the file (the config flag). docs/deviations/Q06.md.

/**
 * @author MrPoke
 */
class _2009ACeremonyinPandaemonium final : public AbstractQuestHandler {
public:
	_2009ACeremonyinPandaemonium() : AbstractQuestHandler(2009) {}

	void register_() override {
		if (CustomConfig::ENABLE_SIMPLE_2NDCLASS)
			return;
		qe.registerOnLevelChanged(questId);
		qe.registerOnQuestCompleted(questId);
		qe.registerQuestNpc(203550)->addOnTalkEvent(questId);
		qe.registerQuestNpc(204182)->addOnTalkEvent(questId);
		qe.registerQuestNpc(204075)->addOnTalkEvent(questId);
		qe.registerQuestNpc(204080)->addOnTalkEvent(questId);
		qe.registerQuestNpc(204081)->addOnTalkEvent(questId);
		qe.registerQuestNpc(204082)->addOnTalkEvent(questId);
		qe.registerQuestNpc(204083)->addOnTalkEvent(questId);
		qe.registerQuestNpc(801220)->addOnTalkEvent(questId);
		qe.registerQuestNpc(801221)->addOnTalkEvent(questId);
	}

	bool onDialogEvent(QuestEnv& env) override {
		runtime::Ptr<Player> player = env.getPlayer();
		runtime::Ptr<QuestState> qs = player->getQuestStateList()->getQuestState(questId);
		if (qs == nullptr)
			return false;

		int32_t var = qs->getQuestVars()->getQuestVars();
		int32_t targetId = 0;
		if (runtime::as<Npc>(env.getVisibleObject()) != nullptr)
			targetId = (runtime::cast<Npc>(env.getVisibleObject()))->getNpcId();

		if (qs->getStatus() == QuestStatus::START) {
			if (targetId == 203550) {
				switch (env.getDialogActionId()) {
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
							TeleportService::teleportTo(*player, 120010000, 1685.0f, 1400.0f, 195.0f, static_cast<int8_t>(0), TeleportAnimation::FADE_OUT_BEAM);
							return true;
						}
				}
			} else if (targetId == 204182) {
				switch (env.getDialogActionId()) {
					case QUEST_SELECT:
						if (var == 1)
							return sendQuestDialog(env, 1352);
						return false;
					case SELECT2_1:
						if (var == 1) {
							playQuestMovie(env, 121);
							return false;
						}
						return false;
					case SETPRO2:
						return defaultCloseDialog(env, 1, 2); // 2
				}
			} else if (targetId == 204075) {
				switch (env.getDialogActionId()) {
					case QUEST_SELECT:
						if (var == 2)
							return sendQuestDialog(env, 1693);
						return false;
					case SELECT3_1:
						if (var == 2) {
							playQuestMovie(env, 122);
							return false;
						}
						return false;
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
				}
			}
		} else if (qs->getStatus() == QuestStatus::REWARD) {
			if (targetId == 204080 && var == 10) {
				switch (env.getDialogActionId()) {
					case USE_OBJECT:
						return sendQuestDialog(env, 2034);
					default:
						return sendQuestEndDialog(env);
				}
			} else if (targetId == 204081 && var == 20) {
				switch (env.getDialogActionId()) {
					case USE_OBJECT:
						return sendQuestDialog(env, 2375);
					default:
						return sendQuestEndDialog(env);
				}
			} else if (targetId == 204082 && var == 30) {
				switch (env.getDialogActionId()) {
					case USE_OBJECT:
						return sendQuestDialog(env, 2716);
					default:
						return sendQuestEndDialog(env);
				}
			} else if (targetId == 204083 && var == 40) {
				switch (env.getDialogActionId()) {
					case USE_OBJECT:
						return sendQuestDialog(env, 3057);
					default:
						return sendQuestEndDialog(env);
				}
			} else if (targetId == 801220 && var == 50) {
				switch (env.getDialogActionId()) {
					case USE_OBJECT:
						return sendQuestDialog(env, 3398);
					default:
						return sendQuestEndDialog(env);
				}
			} else if (targetId == 801221 && var == 60) {
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
		defaultOnLevelChangedEvent(player, {2008});
	}

	void onQuestCompletedEvent(QuestEnv& env) override {
		defaultOnQuestCompletedEvent(env, {2008});
	}
};
AION_QUEST_HANDLER(_2009ACeremonyinPandaemonium, 2009);

} // namespace aion::gameserver::handlers::quest::ascension
