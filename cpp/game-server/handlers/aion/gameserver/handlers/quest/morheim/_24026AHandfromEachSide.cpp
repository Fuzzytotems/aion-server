#include "aion/gameserver/handlers/quest/QuestPrelude.h"

#include <array>

#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/model/gameobjects/player/QuestStateList.h"
#include "aion/gameserver/model/templates/quest/QuestNpc.h"
#include "aion/gameserver/services/QuestService.h"
#include "aion/gameserver/services/teleport/TeleportService.h"
#include "aion/gameserver/world/WorldMapInstance.h"

namespace aion::gameserver::handlers::quest::morheim {

// Hand-ported from game-server/data/handlers/quest/morheim/_24026AHandfromEachSide.java (questgen refuses it: the field the handler
// writes, the int[] varargs, WorldMapInstance.getNpcs), in the questgen output's style, with one correction of the Java code (owner's
// decision 2026-10-13, both branches; docs/deviations/Q09.md, "Correction: 1367 and 24026"): Java's `balaurKilled` (:25) is one counter in
// the singleton handler for every player, so two players defending Kargate at once count each other's kills, and one player's SETPRO3
// resets the other's wave. Here each player's count is the quest's var slot 1 (BALAUR_KILLED_SLOT; slot 0 is the step, as in Java), reset at
// SETPRO3 as Java resets the field; saved and reset with the quest like 2759's record (docs/deviations/Q01.md).

/**
 * Talk with Aegir (204301). Meet Taisan (204403). Pass through Morheim Abyss Gate and talk with Kargate (204423). Protect Kargate from the Balaur:
 * <spawnpos: 254.21326, 256.9302, 226.6418, 93>. Draconute Scout (280818), Crusader (211624), Chandala Scaleguard (213578), Chandala Fangblade
 * (213579). Speak to Kargate. Report back to Aegir.
 *
 * @author Ritsu, Pad
 */
class _24026AHandfromEachSide final : public AbstractQuestHandler {
private:
	static constexpr std::array<int32_t, 2> mobIds{213575, 280818};

public:
	_24026AHandfromEachSide() : AbstractQuestHandler(24026) {}

	void register_() override {
		std::array<int32_t, 3> npcIds{204301, 204403, 204432};
		qe.registerOnQuestCompleted(questId);
		qe.registerOnLevelChanged(questId);
		qe.registerOnQuestTimerEnd(questId);
		qe.registerOnLogOut(questId);
		qe.registerOnEnterWorld(questId);
		for (int32_t npcId : npcIds)
			qe.registerQuestNpc(npcId)->addOnTalkEvent(questId);
		for (int32_t mob : mobIds)
			qe.registerQuestNpc(mob)->addOnKillEvent(questId);
	}

	void onQuestCompletedEvent(QuestEnv& env) override {
		defaultOnQuestCompletedEvent(env, {24025, 24024, 24023, 24022, 24021, 24020});
	}

	void onLevelChangedEvent(Player& player) override {
		defaultOnLevelChangedEvent(player, {24025, 24024, 24023, 24022, 24021, 24020});
	}

	bool onDialogEvent(QuestEnv& env) override {
		runtime::Ptr<Player> player = env.getPlayer();
		runtime::Ptr<QuestState> qs = player->getQuestStateList()->getQuestState(questId);
		if (qs == nullptr)
			return false;
		int32_t var = qs->getQuestVarById(0);
		int32_t targetId = env.getTargetId();
		int32_t dialogActionId = env.getDialogActionId();

		if (qs->getStatus() == QuestStatus::REWARD) {
			if (targetId == 204301) { // Aegir
				if (dialogActionId == USE_OBJECT)
					return sendQuestDialog(env, 2375);
				else
					return sendQuestEndDialog(env);
			}
		} else if (qs->getStatus() == QuestStatus::START) {
			switch (targetId) {
				case 204301: // Aegir
					switch (dialogActionId) {
						case QUEST_SELECT:
							if (var == 0)
								return sendQuestDialog(env, 1011);
							return false;
						case SETPRO1:
							defaultCloseDialog(env, 0, 1); // 1
							TeleportService::teleportTo(*player, 220020000, 2794.55f, 477.6f, 265.65f, static_cast<int8_t>(40), TeleportAnimation::FADE_OUT_BEAM);
							return true;
					}
					break;
				case 204403: // Taisan
					switch (dialogActionId) {
						case QUEST_SELECT:
							if (var == 1)
								return sendQuestDialog(env, 1352);
							return false;
						case SETPRO2:
							defaultCloseDialog(env, 1, 2); // 2
							TeleportService::teleportTo(*player, 220020000, 3030.5f, 875.5f, 363.0f, static_cast<int8_t>(12), TeleportAnimation::FADE_OUT_BEAM);
							return true;
					}
					break;
				case 204432: // Kargate
					switch (dialogActionId) {
						case QUEST_SELECT:
							if (var == 2)
								return sendQuestDialog(env, 1693);
							else if (var == 4)
								return sendQuestDialog(env, 2034);
							return false;
						case SETPRO3:
							qs->setQuestVarById(BALAUR_KILLED_SLOT, 0); // Java: balaurKilled = 0 (:109)
							spawn(213575, *player, 248.78f, 259.28f, 227.74f, static_cast<int8_t>(94)); // Crusader
							spawn(213575, *player, 259.10f, 261.79f, 227.77f, static_cast<int8_t>(94)); // 280818 Draconute Scout
							QuestService::questTimerStart(env, 240);
							return defaultCloseDialog(env, 2, 3); // 3
						case SETPRO4:
							if (var == 4) {
								qs->setRewardGroup(0); // set default reward group to suppress warnings (both possible reward groups are identical)
								defaultCloseDialog(env, 4, 4, true, false); // reward
								TeleportService::teleportTo(*player, 220020000, 3030.8676f, 875.6538f, 363.2065f, static_cast<int8_t>(73), TeleportAnimation::FADE_OUT_BEAM);
								return true;
							}
					}
					break;
			}
		}
		return false;
	}

	bool onKillEvent(QuestEnv& env) override {
		runtime::Ptr<Player> player = env.getPlayer();
		runtime::Ptr<QuestState> qs = player->getQuestStateList()->getQuestState(questId);
		if (qs == nullptr || qs->getStatus() != QuestStatus::START)
			return false;

		if (qs->getQuestVarById(0) == 3) {
			int32_t targetId = env.getTargetId();
			if (targetId == 213575 || targetId == 280818) {
				int32_t balaurKilled = qs->getQuestVarById(BALAUR_KILLED_SLOT) + 1; // Java: balaurKilled++ (:138)
				qs->setQuestVarById(BALAUR_KILLED_SLOT, balaurKilled);
				if (balaurKilled == 2) {
					spawn(213575, *player, 248.78f, 259.28f, 227.74f, static_cast<int8_t>(94)); // Crusader
					spawn(213575, *player, 259.10f, 261.79f, 227.77f, static_cast<int8_t>(94)); // 280818 Draconute Scout
				} else if (balaurKilled == 4) {
					QuestService::questTimerEnd(env);
					if (kargateIsAlive(env)) {
						changeQuestStep(env, 3, 4);
						playQuestMovie(env, 158);
					} else {
						changeQuestStep(env, 3, 2);
						spawn(204432, *player, 272.83f, 176.81f, 204.35f, static_cast<int8_t>(0));
					}
				}
				return true;
			}
		}
		return false;
	}

	bool onQuestTimerEndEvent(QuestEnv& env) override {
		runtime::Ptr<Player> player = env.getPlayer();
		runtime::Ptr<QuestState> qs = player->getQuestStateList()->getQuestState(questId);
		if (qs == nullptr || qs->getStatus() != QuestStatus::START)
			return false;

		if (qs->getQuestVarById(0) == 3) {
			deleteBalaur(env);
			if (kargateIsAlive(env)) {
				changeQuestStep(env, 3, 4);
				playQuestMovie(env, 158);
			} else {
				changeQuestStep(env, 3, 2);
				spawn(204432, *player, 272.83f, 176.81f, 204.35f, static_cast<int8_t>(0));
			}
			return true;
		}
		return false;
	}

	bool onLogOutEvent(QuestEnv& env) override {
		runtime::Ptr<Player> player = env.getPlayer();
		runtime::Ptr<QuestState> qs = player->getQuestStateList()->getQuestState(questId);
		if (qs == nullptr || qs->getStatus() != QuestStatus::START)
			return false;

		if (qs->getQuestVarById(0) == 3) {
			deleteBalaur(env);
			QuestService::questTimerEnd(env);
			changeQuestStep(env, 3, 2);
			if (!kargateIsAlive(env))
				spawn(204432, *player, 272.83f, 176.81f, 204.35f, static_cast<int8_t>(0));
			return true;
		}
		return false;
	}

	bool onEnterWorldEvent(QuestEnv& env) override {
		runtime::Ptr<Player> player = env.getPlayer();
		runtime::Ptr<QuestState> qs = player->getQuestStateList()->getQuestState(questId);
		if (qs == nullptr || qs->getStatus() != QuestStatus::START)
			return false;

		if (qs->getQuestVarById(0) == 3 && player->getWorldId() != 320040000) {
			QuestService::questTimerEnd(env);
			changeQuestStep(env, 3, 2);
			return true;
		}
		return false;
	}

private:
	bool kargateIsAlive(QuestEnv& env) {
		runtime::Ptr<Npc> kargate = env.getPlayer()->getWorldMapInstance()->getNpc(204432);
		return (kargate != nullptr && !kargate->isDead());
	}

	void deleteBalaur(QuestEnv& env) {
		for (const runtime::Ptr<Npc>& npc : env.getPlayer()->getWorldMapInstance()->getNpcs({213575, 280818}))
			npc->getController().delete_();
	}

	/** the quest var slot that counts this player's balaur kills of the defence (slot 0 is the step) */
	static constexpr int32_t BALAUR_KILLED_SLOT = 1;
};
AION_QUEST_HANDLER(_24026AHandfromEachSide, 24026);

} // namespace aion::gameserver::handlers::quest::morheim
