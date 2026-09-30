#include "aion/gameserver/handlers/quest/QuestPrelude.h"

#include <vector>

#include "aion/gameserver/configs/main/CustomConfig.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/controllers/VisibleObjectController.h"
#include "aion/gameserver/controllers/attack/AggroList.h"
#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"
#include "aion/gameserver/model/gameobjects/player/QuestStateList.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/model/templates/quest/QuestNpc.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ASCENSION_MORPH.h"
#include "aion/gameserver/network/aion/serverpackets/SM_DIALOG_WINDOW.h"
#include "aion/gameserver/network/aion/serverpackets/SM_EMOTION.h"
#include "aion/gameserver/questEngine/model/QuestVars.h"
#include "aion/gameserver/services/ClassChangeService.h"
#include "aion/gameserver/services/instance/InstanceService.h"
#include "aion/gameserver/services/reward/WebRewardService.h"
#include "aion/gameserver/services/teleport/TeleportService.h"
#include "aion/gameserver/skillengine/SkillEngine.h"
#include "aion/gameserver/skillengine/model/Effect.h" // the Ref<Effect> applyEffectDirectly returns
#include "aion/gameserver/utils/ThreadPoolManager.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldMapTypeInfo.h"

namespace aion::gameserver::handlers::quest::ascension {

// Hand-ported (lane P6-Q asc-hand, chunk Q06) from game-server/data/handlers/quest/ascension/_2008Ascension.java, statement by statement in
// questgen's conventions; questgen refuses the file (the config flag, two lambdas, SM_ASCENSION_MORPH). docs/deviations/Q06.md.

/**
 * @author MrPoke
 */
class _2008Ascension final : public AbstractQuestHandler {
public:
	_2008Ascension() : AbstractQuestHandler(2008) {}

	void register_() override {
		if (CustomConfig::ENABLE_SIMPLE_2NDCLASS)
			return;
		qe.registerOnLevelChanged(questId);
		qe.registerOnQuestCompleted(questId);
		qe.registerQuestNpc(203550)->addOnTalkEvent(questId);
		qe.registerQuestNpc(790003)->addOnTalkEvent(questId);
		qe.registerQuestNpc(790002)->addOnTalkEvent(questId);
		qe.registerQuestNpc(203546)->addOnTalkEvent(questId);
		qe.registerQuestNpc(205020)->addOnTalkEvent(questId);
		qe.registerQuestNpc(205040)->addOnKillEvent(questId);
		qe.registerQuestNpc(205041)->addOnKillEvent(questId);
		qe.registerOnEnterWorld(questId);
		qe.registerOnDie(questId);
	}

	bool onKillEvent(QuestEnv& env) override {
		runtime::Ptr<Player> player = env.getPlayer();
		runtime::Ptr<QuestState> qs = player->getQuestStateList()->getQuestState(questId);
		if (qs == nullptr || qs->getStatus() != QuestStatus::START)
			return false;

		int32_t var = qs->getQuestVarById(0);
		int32_t targetId = env.getTargetId();
		if (targetId == 205040) { // Guardian Assassin
			env.getVisibleObject()->getController().delete_();
			if (var >= 51 && var <= 53) {
				qs->setQuestVar(qs->getQuestVars()->getQuestVars() + 1);
				updateQuestStatus(env);
				return true;
			} else if (var == 54) {
				qs->setQuestVar(5);
				updateQuestStatus(env);
				runtime::Ptr<Npc> mob = runtime::cast<Npc>(spawn(205041, *player, 301.0f, 259.0f, 205.5f, static_cast<int8_t>(0)));
				mob->getAggroList().addHate(*player, 1000);
				return true;
			}
		} else if (targetId == 205041 && var == 5) {
			playQuestMovie(env, 152);
			player->getWorldMapInstance()->forEachNpc([](Npc& npc) { npc.getController().delete_(); });
			spawn(203550, *player, 301.92999f, 274.26001f, 205.7f, static_cast<int8_t>(0));
			qs->setQuestVar(6);
			updateQuestStatus(env);
			return true;
		}
		return false;
	}

	bool onDialogEvent(QuestEnv& env) override {
		runtime::Ptr<Player> player = env.getPlayer();
		runtime::Ptr<QuestState> qs = player->getQuestStateList()->getQuestState(questId);
		if (qs == nullptr)
			return false;

		int32_t var = qs->getQuestVars()->getQuestVars();
		int32_t targetId = env.getTargetId();

		if (qs->getStatus() == QuestStatus::START) {
			if (targetId == 203550) {
				switch (env.getDialogActionId()) {
					case QUEST_SELECT:
						if (var == 0)
							return sendQuestDialog(env, 1011);
						else if (var == 4)
							return sendQuestDialog(env, 2375);
						else if (var == 6)
							return sendQuestDialog(env, 2716);
						return false;
					case SELECT5_1:
						if (var == 4) {
							playQuestMovie(env, 57);
							removeQuestItem(env, 182203009, 1);
							removeQuestItem(env, 182203010, 1);
							removeQuestItem(env, 182203011, 1);
						}
						return false;
					case SETPRO1: // java-bug kept: no var guard, SETPRO1 at any START var sets var 1 again (docs/deviations/Q06.md)
						qs->setQuestVar(1);
						updateQuestStatus(env);
						TeleportService::teleportTo(*player, 220010000, 585.5074f, 2416.0312f, 278.625f, static_cast<int8_t>(102), TeleportAnimation::FADE_OUT_BEAM);
						return true;
					case SETPRO5:
						if (var == 4) {
							qs->setQuestVar(99);
							updateQuestStatus(env);
							PacketSendUtility::sendPacket(*player, SM_DIALOG_WINDOW(env.getVisibleObject()->getObjectId(), 0));
							// Create instance
							runtime::Ptr<WorldMapInstance> newInstance =
								InstanceService::getNextAvailableInstance(::aion::gameserver::world::getId(WorldMapType::ATAXIAR_B), *player);
							TeleportService::teleportTo(*player, *newInstance, 457.65f, 426.8f, 230.4f);
							return true;
						}
						return false;
					case SETPRO6: {
						int32_t dialogPageId = ClassChangeService::getClassSelectionDialogPageId(player->getRace(), player->getPlayerClass());
						if (var == 6 && dialogPageId != 0)
							return sendQuestDialog(env, dialogPageId);
						return false;
					}
					case SETPRO7:
						return var == 6 && setPlayerClass(env, *qs, PlayerClass::GLADIATOR);
					case SETPRO8:
						return var == 6 && setPlayerClass(env, *qs, PlayerClass::TEMPLAR);
					case SETPRO9:
						return var == 6 && setPlayerClass(env, *qs, PlayerClass::ASSASSIN);
					case SETPRO10:
						return var == 6 && setPlayerClass(env, *qs, PlayerClass::RANGER);
					case SETPRO11:
						return var == 6 && setPlayerClass(env, *qs, PlayerClass::SORCERER);
					case SETPRO12:
						return var == 6 && setPlayerClass(env, *qs, PlayerClass::SPIRIT_MASTER);
					case SETPRO13:
						return var == 6 && setPlayerClass(env, *qs, PlayerClass::CHANTER);
					case SETPRO14:
						return var == 6 && setPlayerClass(env, *qs, PlayerClass::CLERIC);
					case SETPRO15:
						return var == 6 && setPlayerClass(env, *qs, PlayerClass::GUNNER);
					case SETPRO16:
						return var == 6 && setPlayerClass(env, *qs, PlayerClass::BARD);
					case SETPRO17:
						return var == 6 && setPlayerClass(env, *qs, PlayerClass::RIDER);
				}
			} else if (targetId == 790003) {
				switch (env.getDialogActionId()) {
					case QUEST_SELECT:
						if (var == 1)
							return sendQuestDialog(env, 1352);
						return false;
					case SETPRO2:
						if (var == 1) {
							if (player->getInventory().getItemCountByItemId(182203009) == 0)
								giveQuestItem(env, 182203009, 1);
							qs->setQuestVar(2);
							updateQuestStatus(env);
							TeleportService::teleportTo(*player, 220010000, 940.74475f, 2295.5305f, 265.65674f, static_cast<int8_t>(46), TeleportAnimation::FADE_OUT_BEAM);
							return true;
						}
						return false;
				}
			} else if (targetId == 790002) {
				switch (env.getDialogActionId()) {
					case QUEST_SELECT:
						if (var == 2)
							return sendQuestDialog(env, 1693);
						return false;
					case SETPRO3:
						if (var == 2) {
							if (player->getInventory().getItemCountByItemId(182203010) == 0)
								giveQuestItem(env, 182203010, 1);
							qs->setQuestVar(3);
							updateQuestStatus(env);
							TeleportService::teleportTo(*player, 220010000, 1111.5637f, 1719.2745f, 270.114256f, static_cast<int8_t>(114), TeleportAnimation::FADE_OUT_BEAM);
							return true;
						}
						return false;
				}
			} else if (targetId == 203546) {
				switch (env.getDialogActionId()) {
					case QUEST_SELECT:
						if (var == 3)
							return sendQuestDialog(env, 2034);
						return false;
					case SETPRO4:
						if (var == 3) {
							if (player->getInventory().getItemCountByItemId(182203011) == 0)
								giveQuestItem(env, 182203011, 1);
							qs->setQuestVar(4);
							updateQuestStatus(env);
							TeleportService::teleportTo(*player, 220010000, 383.10248f, 1895.3093f, 327.625f, static_cast<int8_t>(59), TeleportAnimation::FADE_OUT_BEAM);
							return true;
						}
						return false;
				}
			} else if (targetId == 205020) {
				switch (env.getDialogActionId()) {
					case QUEST_SELECT:
						if (var == 99) {
							SkillEngine::getInstance().applyEffectDirectly(257, *player, *player);
							player->setState(CreatureState::FLYING);
							player->unsetState(CreatureState::ACTIVE);
							player->setFlightTeleportId(3001);
							PacketSendUtility::sendPacket(*player, SM_EMOTION(*player, EmotionType::START_FLYTELEPORT, 3001, 0));
							qs->setQuestVar(50);
							updateQuestStatus(env);
							// Java lambda _2008Ascension.java:226-237: the task pins the player, the env and the quest state it captured; the
							// handler itself is Immortal (RT-11). java-bug kept: nothing cancels it when he dies or leaves within the 43 s
							// (onDieEvent sets var 4, then this sets 51 and spawns the assassins in whatever map he is in; Q06.md)
							Player& pinnedPlayer = *player;
							QuestState& pinnedQs = *qs;
							ThreadPoolManager::getInstance().schedule({this, &pinnedPlayer, &env, &pinnedQs},
								[this, &pinnedPlayer, &env, &pinnedQs] {
									pinnedQs.setQuestVar(51);
									updateQuestStatus(env);
									std::vector<runtime::Ptr<Npc>> mobs;
									mobs.push_back(runtime::cast<Npc>(spawn(205040, pinnedPlayer, 294.0f, 277.0f, 207.0f, static_cast<int8_t>(0))));
									mobs.push_back(runtime::cast<Npc>(spawn(205040, pinnedPlayer, 305.0f, 279.0f, 206.5f, static_cast<int8_t>(0))));
									mobs.push_back(runtime::cast<Npc>(spawn(205040, pinnedPlayer, 298.0f, 253.0f, 205.7f, static_cast<int8_t>(0))));
									mobs.push_back(runtime::cast<Npc>(spawn(205040, pinnedPlayer, 306.0f, 251.0f, 206.0f, static_cast<int8_t>(0))));
									for (const runtime::Ptr<Npc>& mob : mobs) {
										mob->getAggroList().addHate(pinnedPlayer, 1000);
									}
								},
								43000);
							return true;
						}
						return false;
				}
			}
		} else if (qs->getStatus() == QuestStatus::REWARD) {
			if (targetId == 203550) {
				switch (env.getDialogActionId()) {
					case SELECTED_QUEST_NOREWARD:
						if (player->getWorldId() == 320020000) {
							TeleportService::teleportTo(*player, 220010000, 386.03476f, 1893.9309f, 327.62283f, static_cast<int8_t>(59), TeleportAnimation::FADE_OUT_BEAM);
						}
						break;
				}
				return sendQuestEndDialog(env); // finishes quest or shows reward selection
			}
		}
		return false;
	}

	void onLevelChangedEvent(Player& player) override {
		defaultOnLevelChangedEvent(player);
	}

	bool onEnterWorldEvent(QuestEnv& env) override {
		runtime::Ptr<Player> player = env.getPlayer();
		runtime::Ptr<QuestState> qs = player->getQuestStateList()->getQuestState(questId);
		if (qs != nullptr && qs->getStatus() == QuestStatus::START) {
			int32_t var = qs->getQuestVars()->getQuestVars();
			if (player->getWorldId() == 320020000)
				PacketSendUtility::sendPacket(*player, SM_ASCENSION_MORPH(1));
			else if (var > 4 && var != 6) // 6 is class selection, quest should not reset anymore after you killed hellion
				changeQuestStep(env, var, 4);
		}
		return false;
	}

private:
	bool setPlayerClass(QuestEnv& env, QuestState& qs, PlayerClass playerClass) {
		if (ClassChangeService::setClass(*env.getPlayer(), playerClass)) {
			changeQuestStep(env, 6, 6, true); // reward
			return sendQuestDialog(env, 5);
		}
		return false;
	}

public:
	bool onDieEvent(QuestEnv& env) override {
		runtime::Ptr<Player> player = env.getPlayer();
		runtime::Ptr<QuestState> qs = player->getQuestStateList()->getQuestState(questId);
		if (qs != nullptr && qs->getStatus() == QuestStatus::START && player->getWorldId() == 320020000) {
			int32_t var = qs->getQuestVars()->getQuestVars();
			if (var > 4)
				changeQuestStep(env, var, 4);
		}
		return false;
	}

	// Retail design, confirmed by the owner (2026-09-29: "the Ascension quest would basically put you at maximum EXP at level 9, and then
	// the next quest would bump you over"): QuestService.finishQuest gives the reward's 73,200 exp before this event makes him a Daeva, so
	// a starting class at the level-9 cap stays level 9 with a full bar, and 2009's exp lifts him to level 10 (docs/deviations/Q06.md)
	void onQuestCompletedEvent(QuestEnv& env) override {
		if (env.getQuestId() == questId) {
			runtime::Ptr<Player> player = env.getPlayer();
			player->getCommonData()->updateDaeva();
			if (WebRewardService::MaxLevelReward::isPendingAscension(*player))
				WebRewardService::MaxLevelReward::reward(*player);
		}
	}
};
AION_QUEST_HANDLER(_2008Ascension, 2008);

} // namespace aion::gameserver::handlers::quest::ascension
