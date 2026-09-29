#include "aion/gameserver/handlers/quest/QuestPrelude.h"

#include <array>
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
#include "aion/gameserver/network/aion/serverpackets/SM_EMOTION.h"
#include "aion/gameserver/questEngine/handlers/HandlerResultInfo.h"
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
#include "aion/gameserver/world/zone/ZoneName.h"

namespace aion::gameserver::handlers::quest::ascension {

// Hand-ported (lane P6-Q asc-hand, chunk Q06) from game-server/data/handlers/quest/ascension/_1006Ascension.java, statement by statement in
// questgen's conventions; questgen refuses the file (the config flag, two lambdas, ClassChangeService.setClass). docs/deviations/Q06.md.

/**
 * Talk with Pernos (790001). Go to the island at the center of Cliona Lake (CLIONA_LAKE_210010000) and fill up the bottle Pernos gave you
 * (182200007). Meet Daminu (730008) and obtain Daminu's Essence (182200009). Talk with Pernos. Explore your lost past (310020000, 52, 174, 229).
 * Advance on Karamatis (Belpartan, 205000). Defeat Raiders (211042) (4). Defeat Orissan (211043). Talk with Pernos and choose the path you will take.
 *
 * @author MrPoke, vlog
 */
class _1006Ascension final : public AbstractQuestHandler {
public:
	_1006Ascension() : AbstractQuestHandler(1006) {}

	void register_() override {
		if (CustomConfig::ENABLE_SIMPLE_2NDCLASS)
			return;
		std::array<int32_t, 2> mobs{211042, 211043};
		std::array<int32_t, 3> npcs{790001, 730008, 205000};
		qe.registerOnLevelChanged(questId);
		qe.registerOnQuestCompleted(questId);
		for (int32_t mob : mobs) {
			qe.registerQuestNpc(mob)->addOnKillEvent(questId);
		}
		for (int32_t npc : npcs) {
			qe.registerQuestNpc(npc)->addOnTalkEvent(questId);
		}
		qe.registerQuestItem(182200007, questId);
		qe.registerOnEnterWorld(questId);
		qe.registerOnDie(questId);
	}

	bool onDialogEvent(QuestEnv& env) override {
		runtime::Ptr<Player> player = env.getPlayer();
		runtime::Ptr<QuestState> qs = player->getQuestStateList()->getQuestState(questId);
		if (qs == nullptr)
			return false;
		int32_t dialogActionId = env.getDialogActionId();
		int32_t var = qs->getQuestVarById(0);
		int32_t targetId = 0;
		if (runtime::as<Npc>(env.getVisibleObject()) != nullptr)
			targetId = (runtime::cast<Npc>(env.getVisibleObject()))->getNpcId();

		if (qs->getStatus() == QuestStatus::START) {
			switch (targetId) {
				case 790001: // Pernos
					switch (dialogActionId) {
						case QUEST_SELECT:
							if (var == 0)
								return sendQuestDialog(env, 1011);
							else if (var == 3)
								return sendQuestDialog(env, 1693);
							else if (var == 5)
								return sendQuestDialog(env, 2034);
							return false;
						case SETPRO1: // java-bug kept: no var guard, SETPRO1 at any START var sets var 1 again (docs/deviations/Q06.md)
							if (player->getInventory().getItemCountByItemId(182200007) == 0)
								giveQuestItem(env, 182200007, 1);
							qs->setQuestVar(1);
							updateQuestStatus(env);
							TeleportService::teleportTo(*player, 210010000, 657.0f, 1071.0f, 99.375f, static_cast<int8_t>(72), TeleportAnimation::FADE_OUT_BEAM);
							return true;
						case SETPRO3: { // java-bug kept: no var guard, SETPRO3 at any START var opens a new Karamatis B (docs/deviations/Q06.md)
							runtime::Ptr<WorldMapInstance> newInstance =
								InstanceService::getNextAvailableInstance(::aion::gameserver::world::getId(WorldMapType::KARAMATIS_B), *player);
							TeleportService::teleportTo(*player, *newInstance, 52, 174, 229, static_cast<int8_t>(10), TeleportAnimation::NONE);
							qs->setQuestVar(99); // 99
							updateQuestStatus(env);
							removeQuestItem(env, 182200009, 1);
							return closeDialogWindow(env);
						}
						case SETPRO4: {
							int32_t dialogPageId = ClassChangeService::getClassSelectionDialogPageId(player->getRace(), player->getPlayerClass());
							if (var == 5 && dialogPageId != 0)
								return sendQuestDialog(env, dialogPageId);
							return false;
						}
						case SETPRO5:
							return var == 5 && setPlayerClass(env, *qs, PlayerClass::GLADIATOR);
						case SETPRO6:
							return var == 5 && setPlayerClass(env, *qs, PlayerClass::TEMPLAR);
						case SETPRO7:
							return var == 5 && setPlayerClass(env, *qs, PlayerClass::ASSASSIN);
						case SETPRO8:
							return var == 5 && setPlayerClass(env, *qs, PlayerClass::RANGER);
						case SETPRO9:
							return var == 5 && setPlayerClass(env, *qs, PlayerClass::SORCERER);
						case SETPRO10:
							return var == 5 && setPlayerClass(env, *qs, PlayerClass::SPIRIT_MASTER);
						case SETPRO11:
							return var == 5 && setPlayerClass(env, *qs, PlayerClass::CLERIC);
						case SETPRO12:
							return var == 5 && setPlayerClass(env, *qs, PlayerClass::CHANTER);
						case SETPRO13:
							return var == 5 && setPlayerClass(env, *qs, PlayerClass::GUNNER);
						case SETPRO14:
							return var == 5 && setPlayerClass(env, *qs, PlayerClass::BARD);
						case SETPRO15:
							return var == 5 && setPlayerClass(env, *qs, PlayerClass::RIDER);
					}
					break;
				case 730008: // Daminu
					switch (dialogActionId) {
						case QUEST_SELECT:
							if (var == 2 && player->getInventory().getItemCountByItemId(182200008) >= 1)
								return sendQuestDialog(env, 1352);
							return false;
						case SELECT2_1:
							playQuestMovie(env, 14);
							return sendQuestDialog(env, 1353);
						case SETPRO2:
							if (var == 2) {
								removeQuestItem(env, 182200008, 1);
								giveQuestItem(env, 182200009, 1);
								qs->setQuestVar(3);
								updateQuestStatus(env);
								TeleportService::teleportTo(*player, 210010000, 246.0f, 1639.0f, 100.316f, static_cast<int8_t>(56), TeleportAnimation::FADE_OUT_BEAM);
								return true;
							}
					}
					break;
				case 205000: // Belpartan
					switch (dialogActionId) {
						case QUEST_SELECT:
							if (qs->getQuestVars()->getQuestVars() == 99) {
								SkillEngine::getInstance().applyEffectDirectly(281, *player, *player);
								player->setState(CreatureState::FLYING);
								player->unsetState(CreatureState::ACTIVE);
								player->setFlightTeleportId(1001);
								PacketSendUtility::sendPacket(*player, SM_EMOTION(*player, EmotionType::START_FLYTELEPORT, 1001, 0));
								qs->setQuestVar(50); // 50
								updateQuestStatus(env);
								// Java lambda _1006Ascension.java:164-175: the task pins the player, the env and the quest state it captured; the
								// handler itself is Immortal (RT-11). java-bug kept: nothing cancels it when he dies or leaves within the 43 s
								// (onDieEvent sets var 3, then this sets 51 and spawns the raiders in whatever map he is in; Q06.md)
								Player& pinnedPlayer = *player;
								QuestState& pinnedQs = *qs;
								ThreadPoolManager::getInstance().schedule({this, &pinnedPlayer, &env, &pinnedQs},
									[this, &pinnedPlayer, &env, &pinnedQs] {
										pinnedQs.setQuestVar(51);
										updateQuestStatus(env);
										std::vector<runtime::Ptr<Npc>> mobs;
										mobs.push_back(runtime::cast<Npc>(spawn(211042, pinnedPlayer, 224.073f, 239.1f, 206.7f, static_cast<int8_t>(0))));
										mobs.push_back(runtime::cast<Npc>(spawn(211042, pinnedPlayer, 233.5f, 241.04f, 206.365f, static_cast<int8_t>(0))));
										mobs.push_back(runtime::cast<Npc>(spawn(211042, pinnedPlayer, 229.6f, 265.7f, 205.7f, static_cast<int8_t>(0))));
										mobs.push_back(runtime::cast<Npc>(spawn(211042, pinnedPlayer, 222.8f, 262.5f, 205.7f, static_cast<int8_t>(0))));
										for (const runtime::Ptr<Npc>& mob : mobs) {
											mob->getAggroList().addHate(pinnedPlayer, 1000);
										}
									},
									43000);
								return true;
							}
					}
			}
		} else if (qs->getStatus() == QuestStatus::REWARD) {
			if (targetId == 790001) { // Pernos
				switch (env.getDialogActionId()) {
					case SELECTED_QUEST_NOREWARD:
						if (player->getWorldId() == 310020000)
							TeleportService::teleportTo(*player, 210010000, 245.14868f, 1639.1372f, 100.35713f, static_cast<int8_t>(60), TeleportAnimation::FADE_OUT_BEAM);
						break;
				}
				return sendQuestEndDialog(env); // finishes quest or shows reward selection
			}
		}
		return false;
	}

	HandlerResult onItemUseEvent(QuestEnv& env, Item& item) override {
		runtime::Ptr<Player> player = env.getPlayer();
		runtime::Ptr<QuestState> qs = player->getQuestStateList()->getQuestState(questId);
		if (qs != nullptr && qs->getStatus() == QuestStatus::START) {
			if (player->isInsideItemUseZone(ZoneName::get("LF1_ITEMUSEAREA_Q1006"))) {
				int32_t var = qs->getQuestVarById(0);
				if (var == 1) {
					return ::aion::gameserver::questEngine::handlers::fromBoolean(useQuestItem(env, item, 1, 2, false, 182200008, 1, 0)); // 2
				}
			}
		}
		return HandlerResult::SUCCESS; // ??
	}

	bool onKillEvent(QuestEnv& env) override {
		runtime::Ptr<Player> player = env.getPlayer();
		runtime::Ptr<QuestState> qs = player->getQuestStateList()->getQuestState(questId);
		if (qs != nullptr && qs->getStatus() == QuestStatus::START) {
			int32_t var = qs->getQuestVarById(0);
			int32_t targetId = env.getTargetId();
			if (targetId == 211042) {
				env.getVisibleObject()->getController().delete_();
				if (var >= 51 && var < 54) {
					return defaultOnKillEvent(env, 211042, 51, 54); // 52 - 54
				} else if (var == 54) {
					qs->setQuestVar(4); // 4
					updateQuestStatus(env);
					runtime::Ptr<Npc> mob = runtime::cast<Npc>(spawn(211043, *player, 226.7f, 251.5f, 205.5f, static_cast<int8_t>(0)));
					mob->getAggroList().addHate(*player, 1000);
					return true;
				}
			} else if (targetId == 211043 && var == 4) {
				playQuestMovie(env, 151);
				player->getWorldMapInstance()->forEachNpc([](Npc& npc) { npc.getController().delete_(); });
				spawn(790001, *player, 220.6f, 247.8f, 206.0f, static_cast<int8_t>(0));
				qs->setQuestVar(5); // 5
				updateQuestStatus(env);
			}
		}
		return false;
	}

private:
	bool setPlayerClass(QuestEnv& env, QuestState& qs, PlayerClass playerClass) {
		if (ClassChangeService::setClass(*env.getPlayer(), playerClass)) {
			changeQuestStep(env, 5, 5, true); // reward
			return sendQuestDialog(env, 5);
		}
		return false;
	}

public:
	bool onDieEvent(QuestEnv& env) override {
		runtime::Ptr<Player> player = env.getPlayer();
		runtime::Ptr<QuestState> qs = player->getQuestStateList()->getQuestState(questId);
		if (qs != nullptr && qs->getStatus() == QuestStatus::START && player->getWorldId() == 310020000) {
			int32_t var = qs->getQuestVars()->getQuestVars();
			if (var > 3)
				changeQuestStep(env, var, 3);
		}
		return false;
	}

	bool onEnterWorldEvent(QuestEnv& env) override {
		runtime::Ptr<Player> player = env.getPlayer();
		runtime::Ptr<QuestState> qs = player->getQuestStateList()->getQuestState(questId);
		if (qs != nullptr && qs->getStatus() == QuestStatus::START) {
			int32_t var = qs->getQuestVars()->getQuestVars();
			if (player->getWorldId() == 310020000)
				PacketSendUtility::sendPacket(*player, SM_ASCENSION_MORPH(1));
			else if (var > 3 && var != 5) // 5 is class selection, quest should not reset anymore after you killed orissan
				changeQuestStep(env, var, 3);
		}
		return false;
	}

	void onLevelChangedEvent(Player& player) override {
		defaultOnLevelChangedEvent(player);
	}

	// java-bug kept: QuestService.finishQuest gives the reward's 73,200 exp before this event makes him a Daeva, so a starting class at the
	// level-9 cap loses it and stays level 9 with a full bar until his next exp (docs/deviations/Q06.md)
	void onQuestCompletedEvent(QuestEnv& env) override {
		if (env.getQuestId() == questId) {
			runtime::Ptr<Player> player = env.getPlayer();
			player->getCommonData()->updateDaeva();
			if (WebRewardService::MaxLevelReward::isPendingAscension(*player))
				WebRewardService::MaxLevelReward::reward(*player);
		}
	}
};
AION_QUEST_HANDLER(_1006Ascension, 1006);

} // namespace aion::gameserver::handlers::quest::ascension
