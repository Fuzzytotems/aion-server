#include "aion/gameserver/handlers/quest/QuestPrelude.h"

#include <array>

#include "aion/gameserver/model/gameobjects/player/QuestStateList.h"
#include "aion/gameserver/model/templates/VisibleObjectTemplate.h"
#include "aion/gameserver/model/templates/quest/QuestNpc.h"

namespace aion::gameserver::handlers::quest::ishalgen {

// Hand-ported from game-server/data/handlers/quest/ishalgen/_2004ACharmedCube.java (questgen refuses it: VisibleObject.getObjectTemplate,
// which C++ has at VisibleObject.h:172), statement by statement in the questgen output's style (docs/deviations/Q09-hand.md).

/**
 * @author Mr. Poke, vlog, Majka
 */
class _2004ACharmedCube final : public AbstractQuestHandler {
private:
	// fieldmap: a table of literals that is never written (Java's effectively final int[]), questgen's static constexpr std::array
	static constexpr std::array<int32_t, 2> mobs{210402, 210403};

public:
	_2004ACharmedCube() : AbstractQuestHandler(2004) {}

	void register_() override {
		std::array<int32_t, 3> npcs{203539, 700047, 203550};
		qe.registerOnQuestCompleted(questId);
		qe.registerOnLevelChanged(questId);
		for (int32_t npc : npcs) {
			qe.registerQuestNpc(npc)->addOnTalkEvent(questId);
		}
		for (int32_t mob : mobs) {
			qe.registerQuestNpc(mob)->addOnKillEvent(questId);
		}
	}

	bool onDialogEvent(QuestEnv& env) override {
		runtime::Ptr<Player> player = env.getPlayer();
		runtime::Ptr<QuestState> qs = player->getQuestStateList()->getQuestState(questId);
		if (qs == nullptr)
			return false;
		int32_t var = qs->getQuestVarById(0);
		int32_t targetId = env.getTargetId();
		int32_t dialogActionId = env.getDialogActionId();

		if (qs->getStatus() == QuestStatus::START) {
			switch (targetId) {
				case 203539: // Derot
					switch (dialogActionId) {
						case QUEST_SELECT:
							if (var == 0) {
								return sendQuestDialog(env, 1011);
							} else if (var == 1) {
								return sendQuestDialog(env, 1352);
							}
							return false;
						case SETPRO1:
							return defaultCloseDialog(env, 0, 1); // 1
						case SETPRO2:
							giveQuestItem(env, 182203005, 1);
							return sendQuestSelectionDialog(env);
						case CHECK_USER_HAS_QUEST_ITEM:
							return checkQuestItems(env, 1, 2, false, 1438, 1353); // 2
						case FINISH_DIALOG:
							return sendQuestSelectionDialog(env);
					}
					break;
				case 700047: // Tombstone
					if (var == 1 && env.getVisibleObject()->getObjectTemplate()->getTemplateId() == 700047 && dialogActionId == USE_OBJECT) {
						spawnForFiveMinutesInFront(211755, *env.getVisibleObject(), env.getVisibleObject()->getHeading(), 2);
						return true;
					}
					return false;
				case 203550: // Munin
					switch (dialogActionId) {
						case QUEST_SELECT:
							if (var == 2) {
								return sendQuestDialog(env, 1693);
							} else if (var == 6) {
								return sendQuestDialog(env, 2034);
							}
							return false;
						case SETPRO3:
							return defaultCloseDialog(env, 2, 3, 0, 0, 182203005, 1); // 3
						case SETPRO4:
							return defaultCloseDialog(env, 6, 6, true, false); // reward
					}
			}
		} else if (qs->getStatus() == QuestStatus::REWARD) {
			if (targetId == 203539) { // Derot
				if (dialogActionId == USE_OBJECT) {
					return sendQuestDialog(env, 2375);
				} else {
					return sendQuestEndDialog(env);
				}
			}
		}
		return false;
	}

	bool onKillEvent(QuestEnv& env) override {
		return defaultOnKillEvent(env, mobs, 3, 6); // 3 - 6
	}

	void onQuestCompletedEvent(QuestEnv& env) override {
		defaultOnQuestCompletedEvent(env, {2100});
	}

	void onLevelChangedEvent(Player& player) override {
		defaultOnLevelChangedEvent(player, {2100});
	}
};
AION_QUEST_HANDLER(_2004ACharmedCube, 2004);

} // namespace aion::gameserver::handlers::quest::ishalgen
