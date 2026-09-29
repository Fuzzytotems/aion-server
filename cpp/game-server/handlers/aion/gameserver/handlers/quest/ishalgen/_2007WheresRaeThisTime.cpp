#include "aion/gameserver/handlers/quest/QuestPrelude.h"

#include <array>

#include "aion/gameserver/model/gameobjects/player/QuestStateList.h"
#include "aion/gameserver/model/templates/quest/QuestNpc.h"
#include "aion/gameserver/services/teleport/TeleportService.h"

namespace aion::gameserver::handlers::quest::ishalgen {

// Hand-ported from game-server/data/handlers/quest/ishalgen/_2007WheresRaeThisTime.java (questgen refuses it: TeleportService.teleportToNpc,
// which this lane ported under a lease of TeleportService.cpp), statement by statement in the questgen output's style
// (docs/deviations/Q09.md, Hand ports).

/**
 * @author Mr. Poke, Majka
 */
class _2007WheresRaeThisTime final : public AbstractQuestHandler {
public:
	_2007WheresRaeThisTime() : AbstractQuestHandler(2007) {}

	void register_() override {
		std::array<int32_t, 8> talkNpcs{203516, 203519, 203539, 203552, 203554, 700085, 700086, 700087};
		qe.registerOnQuestCompleted(questId);
		qe.registerOnLevelChanged(questId);
		for (int32_t id : talkNpcs)
			qe.registerQuestNpc(id)->addOnTalkEvent(questId);
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
				case 203516:
					switch (dialogActionId) {
						case QUEST_SELECT:
							if (var == 0)
								return sendQuestDialog(env, 1011);
							return false;
						case SETPRO1:
							if (var == 0) {
								qs->setQuestVarById(0, var + 1);
								updateQuestStatus(env);
								closeDialogWindow(env);
								return true;
							}
					}
					break;
				case 203519:
					switch (dialogActionId) {
						case QUEST_SELECT:
							if (var == 1)
								return sendQuestDialog(env, 1352);
							return false;
						case SETPRO2:
							if (var == 1) {
								qs->setQuestVarById(0, var + 1);
								updateQuestStatus(env);
								closeDialogWindow(env);
								return true;
							}
					}
					break;
				case 203539:
					switch (dialogActionId) {
						case QUEST_SELECT:
							if (var == 2)
								return sendQuestDialog(env, 1693);
							return false;
						case SELECT3_1:
							playQuestMovie(env, 55);
							break;
						case SETPRO3:
							if (var == 2) {
								qs->setQuestVarById(0, var + 1);
								updateQuestStatus(env);
								closeDialogWindow(env);
								return true;
							}
					}
					break;
				case 203552:
					switch (dialogActionId) {
						case QUEST_SELECT:
							if (var == 3)
								return sendQuestDialog(env, 2034);
							return false;
						case SETPRO4:
							if (var == 3) {
								qs->setQuestVarById(0, var + 1);
								updateQuestStatus(env);
								closeDialogWindow(env);
								return true;
							}
					}
					break;
				case 203554:
					switch (dialogActionId) {
						case QUEST_SELECT:
							if (var == 4)
								return sendQuestDialog(env, 2375);
							else if (var == 8)
								return sendQuestDialog(env, 2716);
							return false;
						case SETPRO5:
							if (var == 4) {
								qs->setQuestVar(5);
								updateQuestStatus(env);
								closeDialogWindow(env);
								return true;
							}
							break;
						case SETPRO6:
							if (var == 8) {
								qs->setQuestVar(9);
								updateQuestStatus(env);
								qs->setQuestVar(8);
								qs->setStatus(QuestStatus::REWARD);
								updateQuestStatus(env);
								closeDialogWindow(env);
								TeleportService::teleportToNpc(*player, 203516);
								return true;
							}
					}
					break;
				case 700085:
					if (var == 5) {
						destroy(6, env);
						return false;
					}
					break;
				case 700086:
					if (var == 6) {
						destroy(7, env);
						return false;
					}
					break;
				case 700087:
					if (var == 7) {
						destroy(-1, env);
						return false;
					}
					break;
			}
		} else if (qs->getStatus() == QuestStatus::REWARD) {
			if (targetId == 203516) {
				if (dialogActionId == USE_OBJECT) {
					playQuestMovie(env, 58);
					return sendQuestDialog(env, 3057);
				} else
					return sendQuestEndDialog(env);
			}
		}
		return false;
	}

	void onQuestCompletedEvent(QuestEnv& env) override {
		defaultOnQuestCompletedEvent(env, {2006, 2005, 2004, 2003, 2002, 2001});
	}

	void onLevelChangedEvent(Player& player) override {
		defaultOnLevelChangedEvent(player, {2100, 2006, 2005, 2004, 2003, 2002, 2001});
	}

private:
	void destroy(int32_t var, QuestEnv& env) {
		runtime::Ptr<Player> player = env.getPlayer();
		// sendEmotion(env, player, EmotionId.STAND, true); //wrong emotion and source of it - rechk on retail
		runtime::Ptr<QuestState> qs = player->getQuestStateList()->getQuestState(questId);
		switch (var) {
			case 6:
			case 7:
				qs->setQuestVar(var);
				break;
			case -1:
				playQuestMovie(env, 56);
				qs->setQuestVar(8);
				break;
		}
		updateQuestStatus(env);
	}
};
AION_QUEST_HANDLER(_2007WheresRaeThisTime, 2007);

} // namespace aion::gameserver::handlers::quest::ishalgen
