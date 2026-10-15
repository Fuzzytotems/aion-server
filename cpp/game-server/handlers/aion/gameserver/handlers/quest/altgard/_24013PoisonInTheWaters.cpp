#include "aion/gameserver/handlers/quest/QuestPrelude.h"

#include <array>

#include "aion/gameserver/model/gameobjects/player/QuestStateList.h"
#include "aion/gameserver/model/templates/quest/QuestNpc.h"
#include "aion/gameserver/questEngine/handlers/HandlerResultInfo.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"
#include "aion/gameserver/world/zone/ZoneName.h"

namespace aion::gameserver::handlers::quest::altgard {

// Hand-ported from game-server/data/handlers/quest/altgard/_24013PoisonInTheWaters.java (questgen refuses it: the lambda of :85), statement by
// statement in the questgen output's style (docs/deviations/Q10.md, Hand ports).

/**
 * @author Artur, Ritsu, Majka
 */
class _24013PoisonInTheWaters final : public AbstractQuestHandler {
private:
	static constexpr std::array<int32_t, 5> mobs{210455, 210456, 214039, 210458, 214032};

public:
	_24013PoisonInTheWaters() : AbstractQuestHandler(24013) {}

	void register_() override {
		qe.registerOnQuestCompleted(questId);
		qe.registerOnLevelChanged(questId);
		for (int32_t mob : mobs) {
			qe.registerQuestNpc(mob)->addOnKillEvent(questId);
		}
		qe.registerQuestItem(182215359, questId);
		qe.registerQuestNpc(203631)->addOnTalkEvent(questId);
		qe.registerQuestNpc(203621)->addOnTalkEvent(questId);
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
				case 203631:
					switch (dialogActionId) {
						case QUEST_SELECT:
							if (var == 0)
								return sendQuestDialog(env, 1011);
							break;
						case SETPRO1:
							return defaultCloseDialog(env, 0, 1); // 1
					}
					return false;
				case 203621:
					switch (dialogActionId) {
						case QUEST_SELECT:
							if (var == 1)
								return sendQuestDialog(env, 1352);
							break;
						case SETPRO2:
							return defaultCloseDialog(env, 1, 2, 182215359, 1); // 2
					}
			}
		} else if (qs->getStatus() == QuestStatus::REWARD) {
			if (targetId == 203631) {
				return sendQuestEndDialog(env);
			}
		}
		return false;
	}

	HandlerResult onItemUseEvent(QuestEnv& env, Item& item) override {
		runtime::Ptr<Player> player = env.getPlayer();
		if (player->isInsideZone(ZoneName::get("DF1A_ITEMUSEAREA_Q2016_220030000"))) {
			// Spawns 2 Feral Black Claw Sharpeye [ID: 210457] far from the player
			// the lambda at _24013PoisonInTheWaters.java:85 (fieldmap _24013PoisonInTheWaters@L85:45, storage: task): its captures - this handler
			// (Immortal, RT-11; spawn is its helper), the env and the player - are the Pin of the task
			Player& user = *player;
			ThreadPoolManager::getInstance().schedule({this, &env, &user},
				[this, &env, &user] {
					float playerX = env.getPlayer()->getX();
					float playerY = env.getPlayer()->getY();
					float playerZ = env.getPlayer()->getZ();
					spawn(210457, user, playerX + 13.0f, playerY - 3.0f, playerZ, static_cast<int8_t>(60)); // Right
					spawn(210457, user, playerX + 13.0f, playerY + 3.0f, playerZ, static_cast<int8_t>(60)); // Left
				},
				3000);

			return ::aion::gameserver::questEngine::handlers::fromBoolean(useQuestItem(env, item, 2, 3, false)); // 3
		}
		return HandlerResult::UNKNOWN;
	}

	bool onKillEvent(QuestEnv& env) override {
		if (defaultOnKillEvent(env, mobs, 7, true))
			return true;
		return defaultOnKillEvent(env, mobs, 3, 7); // 6
	}

	void onQuestCompletedEvent(QuestEnv& env) override {
		defaultOnQuestCompletedEvent(env, {24010});
	}

	void onLevelChangedEvent(Player& player) override {
		defaultOnLevelChangedEvent(player, {24010});
	}
};
AION_QUEST_HANDLER(_24013PoisonInTheWaters, 24013);

} // namespace aion::gameserver::handlers::quest::altgard
