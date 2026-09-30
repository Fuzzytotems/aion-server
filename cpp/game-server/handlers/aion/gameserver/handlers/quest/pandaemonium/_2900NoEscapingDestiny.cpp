#include "aion/gameserver/handlers/quest/QuestPrelude.h"

#include <array>
#include <string>

#include "aion/gameserver/dataholders/loadingutils/EnumTraits.h"
#include "aion/gameserver/model/DialogPageInfo.h"
#include "aion/gameserver/model/PlayerClass.h"
#include "aion/gameserver/model/gameobjects/player/Equipment.h"
#include "aion/gameserver/model/gameobjects/player/QuestStateList.h"
#include "aion/gameserver/model/templates/quest/QuestNpc.h"
#include "aion/gameserver/network/aion/serverpackets/SM_DIALOG_WINDOW.h"
#include "aion/gameserver/questEngine/model/QuestVars.h"
#include "aion/gameserver/services/instance/InstanceService.h"
#include "aion/gameserver/services/teleport/TeleportService.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldMapTypeInfo.h"

namespace aion::gameserver::handlers::quest::pandaemonium {

// Hand-ported from game-server/data/handlers/quest/pandaemonium/_2900NoEscapingDestiny.java (questgen refuses it: the throw of :259 and
// Equipment.unEquipItem, not in its API table), statement by statement in the questgen output's style (docs/deviations/Q10.md, Hand
// ports).

/**
 * @author Mr. Poke, Rolandas, vlog
 */
class _2900NoEscapingDestiny final : public AbstractQuestHandler {
public:
	_2900NoEscapingDestiny() : AbstractQuestHandler(2900) {}

	void register_() override {
		std::array<int32_t, 7> npcs{204182, 203550, 790003, 790002, 203546, 204264, 204061};
		std::array<int32_t, 4> stigmas{140000001, 140000002, 140000003, 140000004};
		qe.registerOnLevelChanged(questId);
		qe.registerQuestNpc(204263)->addOnKillEvent(questId);
		qe.registerOnEnterWorld(questId);
		qe.registerOnDie(questId);
		for (int32_t npc : npcs) {
			qe.registerQuestNpc(npc)->addOnTalkEvent(questId);
		}
		for (int32_t stigma : stigmas) {
			qe.registerOnEquipItem(stigma, questId);
		}
	}

	bool onDialogEvent(QuestEnv& env) override {
		runtime::Ptr<Player> player = env.getPlayer();
		runtime::Ptr<QuestState> qs = player->getQuestStateList()->getQuestState(questId);
		if (qs == nullptr) {
			return false;
		}
		int32_t var = qs->getQuestVars()->getQuestVars();
		int32_t targetId = env.getTargetId();
		int32_t dialogActionId = env.getDialogActionId();

		if (qs->getStatus() == QuestStatus::START) {
			switch (targetId) {
				case 204182: // Heimdall
					switch (dialogActionId) {
						case QUEST_SELECT:
							if (var == 0) {
								return sendQuestDialog(env, 1011);
							}
							return false;
						case SETPRO1:
							if (defaultCloseDialog(env, 0, 1)) { // 1
								TeleportService::teleportTo(*player, 220010000, 1, 389.0f, 1896.0f, 327.5f, static_cast<int8_t>(61), TeleportAnimation::FADE_OUT_BEAM);
								return true;
							}
					}
					break;
				case 203550: // Munin
					switch (dialogActionId) {
						case QUEST_SELECT:
							if (var == 1) {
								return sendQuestDialog(env, 1352);
							} else if (var == 10) {
								return sendQuestDialog(env, 4080);
							}
							return false;
						case SETPRO2:
							return defaultCloseDialog(env, 1, 2); // 2
						case SETPRO10:
							defaultCloseDialog(env, 10, 10, true, false); // reward
							TeleportService::teleportTo(*player, 120010000, 1294.8f, 1213.8f, 214.34f, static_cast<int8_t>(30), TeleportAnimation::FADE_OUT_BEAM);
							return true;
					}
					break;
				case 790003: // Urd
					switch (dialogActionId) {
						case QUEST_SELECT:
							if (var == 2) {
								return sendQuestDialog(env, 1693);
							}
							return false;
						case SETPRO3:
							return defaultCloseDialog(env, 2, 3); // 3
					}
					break;
				case 790002: // Verdandi
					switch (dialogActionId) {
						case QUEST_SELECT:
							if (var == 3) {
								return sendQuestDialog(env, 2034);
							}
							return false;
						case SETPRO4:
							return defaultCloseDialog(env, 3, 4); // 4
					}
					break;
				case 203546: // Skuld
					switch (dialogActionId) {
						case QUEST_SELECT:
							if (var == 4) {
								return sendQuestDialog(env, 2375);
							} else if (var == 9) {
								return sendQuestDialog(env, 3739);
							}
							return false;
						case SETPRO5:
							if (var == 4) {
								changeQuestStep(env, 4, 95); // 95
								runtime::Ptr<WorldMapInstance> newInstance = InstanceService::getNextAvailableInstance(::aion::gameserver::world::getId(WorldMapType::SPACE_OF_DESTINY), *player);
								TeleportService::teleportTo(*player, *newInstance, 270.8424f, 249.1182f, 125.8369f, static_cast<int8_t>(60), TeleportAnimation::FADE_OUT_BEAM);
								return closeDialogWindow(env);
							}
							return false;
						case SETPRO9:
							changeQuestStep(env, 9, 10); // 10
							TeleportService::teleportTo(*player, 220010000, 1, 383.0f, 1896.0f, 327.625f, static_cast<int8_t>(60), TeleportAnimation::FADE_OUT_BEAM);
							return closeDialogWindow(env);
					}
					break;
				case 204264: // Skuld 2
					switch (dialogActionId) {
						case QUEST_SELECT:
							if (var == 95) {
								return sendQuestDialog(env, 2716);
							} else if (var == 96 || var == 99) {
								return sendQuestDialog(env, 3057);
							} else if (var == 97) {
								return sendQuestDialog(env, 3398);
							}
							return false;
						case SETPRO6:
							if (var == 95) {
								playQuestMovie(env, 156);
								return closeDialogWindow(env);
							}
							return false;
						case SELECT7_1:
							if (giveQuestItem(env, getStoneId(player), 1))
								changeQuestStep(env, 96, 99); // 99
							return sendQuestDialog(env, 3058);
						case SETPRO7:
							if (var == 99) {
								PacketSendUtility::sendPacket(*player, SM_DIALOG_WINDOW(env.getVisibleObject()->getObjectId(), ::aion::gameserver::model::id(DialogPage::STIGMA)));
								return true;
							}
							return false;
						case SETPRO8:
							if (var == 97) {
								changeQuestStep(env, 97, 98); // 98
								spawnForFiveMinutes(204263, *player->getWorldMapInstance(), 257.5f, 245.0f, 125.0f, static_cast<int8_t>(0));
								return closeDialogWindow(env);
							}
					}
					break;
			}
		} else if (qs->getStatus() == QuestStatus::REWARD) {
			if (targetId == 204061) { // Aud
				return sendQuestEndDialog(env);
			}
		}
		return false;
	}

	void onMovieEndEvent(QuestEnv& env, int32_t movieId) override {
		if (movieId == 156)
			changeQuestStep(env, 95, 96);
	}

	bool onEquipItemEvent(QuestEnv& env, int32_t itemId) override {
		changeQuestStep(env, 99, 97); // 97
		return closeDialogWindow(env);
	}

	bool onKillEvent(QuestEnv& env) override {
		runtime::Ptr<Player> player = env.getPlayer();
		runtime::Ptr<QuestState> qs = player->getQuestStateList()->getQuestState(questId);
		if (qs != nullptr && qs->getStatus() == QuestStatus::START) {
			int32_t var = qs->getQuestVars()->getQuestVars();
			if (var == 98) {
				changeQuestStep(env, 98, 9); // 9
				TeleportService::teleportTo(*player, 220010000, 1, 1112.492f, 1718.974f, 270.45917f, static_cast<int8_t>(113), TeleportAnimation::FADE_OUT_BEAM);
				return true;
			}
		}
		return false;
	}

	bool onDieEvent(QuestEnv& env) override {
		runtime::Ptr<Player> player = env.getPlayer();
		runtime::Ptr<QuestState> qs = player->getQuestStateList()->getQuestState(questId);
		if (qs != nullptr && qs->getStatus() == QuestStatus::START) {
			int32_t var = qs->getQuestVars()->getQuestVars();
			if (var >= 95 && var <= 99) {
				removeStigma(env);
				changeQuestStep(env, var, 4);
				return true;
			}
		}
		return false;
	}

	bool onEnterWorldEvent(QuestEnv& env) override {
		runtime::Ptr<Player> player = env.getPlayer();
		runtime::Ptr<QuestState> qs = player->getQuestStateList()->getQuestState(questId);
		if (qs != nullptr && qs->getStatus() == QuestStatus::START) {
			int32_t var = qs->getQuestVars()->getQuestVars();
			if (player->getWorldId() != 320070000) {
				if (var >= 95 && var <= 99) {
					removeStigma(env);
					changeQuestStep(env, var, 4);
					return true;
				} else if (var == 9) {
					removeStigma(env);
					return true;
				}
			}
		}
		return false;
	}

private:
	int32_t getStoneId(runtime::Ptr<Player> player) {
		// TODO: find out the correct stigma ids for each class on official servers
		switch (player->getPlayerClass()) {
			case PlayerClass::CHANTER:
			case PlayerClass::CLERIC:
			case PlayerClass::BARD:
				return 140000001; // Healight Light II
			case PlayerClass::RIDER:
			case PlayerClass::GUNNER:
			case PlayerClass::RANGER:
				return 140000002; // Flame Cage I
			case PlayerClass::GLADIATOR:
			case PlayerClass::ASSASSIN:
			case PlayerClass::TEMPLAR:
				return 140000003; // Ferocious Strike III (melee weapon required)
			case PlayerClass::SORCERER:
			case PlayerClass::SPIRIT_MASTER:
				return 140000004; // Hydro Eruption II
			default:
				throw runtime::UnsupportedOperationException("Unhandled player class " + std::string(::aion::gameserver::xml::enumName(player->getPlayerClass())));
		}
	}

	void removeStigma(QuestEnv& env) {
		runtime::Ptr<Player> player = env.getPlayer();
		int32_t stigmaId = getStoneId(player);
		for (runtime::Ptr<Item> item : player->getEquipment().getEquippedItemsByItemId(stigmaId)) {
			player->getEquipment().unEquipItem(item->getObjectId());
		}
		removeQuestItem(env, stigmaId, 1);
	}

public:
	void onLevelChangedEvent(Player& player) override {
		defaultOnLevelChangedEvent(player);
	}
};
AION_QUEST_HANDLER(_2900NoEscapingDestiny, 2900);

} // namespace aion::gameserver::handlers::quest::pandaemonium
