#include "aion/gameserver/handlers/quest/QuestPrelude.h"

#include "aion/gameserver/model/gameobjects/player/QuestStateList.h"
#include "aion/gameserver/model/templates/quest/QuestItems.h"
#include "aion/gameserver/model/templates/quest/QuestNpc.h"
#include "aion/gameserver/services/QuestService.h"

namespace aion::gameserver::handlers::quest::event_quests {

// Hand-ported from game-server/data/handlers/quest/event_quests/_80016EventSockHop.java (questgen refuses it: `rewardItems.add(new QuestItems(...))` on onBonusApplyEvent's list), in the questgen output's
// style: drafted by transliterating the copy without that line, then the hat box added by hand to the list the hook now passes mutable (m5d-h01). No correction of the Java code (lane C, 2026-10-18).

/**
 * @author Rolandas
 */
class _80016EventSockHop final : public AbstractQuestHandler {
public:
	_80016EventSockHop() : AbstractQuestHandler(80016) {}

	void register_() override {
		qe.registerQuestNpc(799763)->addOnQuestStart(questId);
		qe.registerQuestNpc(799763)->addOnTalkEvent(questId);
		qe.registerOnBonusApply(questId, BonusType::MOVIE);
	}

	bool onDialogEvent(QuestEnv& env) override {
		runtime::Ptr<Player> player = env.getPlayer();
		runtime::Ptr<QuestState> qs = player->getQuestStateList()->getQuestState(questId);
		if (qs == nullptr || qs->isStartable()) {
			if (env.getTargetId() == 799763) {
				switch (env.getDialogActionId()) {
					case USE_OBJECT:
						return sendQuestDialog(env, 1011);
					case QUEST_ACCEPT_1:
						QuestService::startEventQuest(env, QuestStatus::START);
						return sendQuestDialog(env, 1003);
					default:
						return sendQuestStartDialog(env);
				}
			}
			return false;
		}

		int32_t var = qs->getQuestVarById(0);

		if (qs->getStatus() == QuestStatus::START) {
			if (env.getTargetId() == 799763) {
				switch (env.getDialogActionId()) {
					case USE_OBJECT:
					case QUEST_SELECT:
						if (var == 0)
							return sendQuestDialog(env, 2375);
						return false;
					case CHECK_USER_HAS_QUEST_ITEM:
						return checkQuestItems(env, 0, 1, true, 5, 2716);
				}
			}
		}

		return sendQuestRewardDialog(env, 799763, 0);
	}

	HandlerResult onBonusApplyEvent(QuestEnv& env, BonusType bonusType, std::vector<QuestItems>& rewardItems) override {
		if (bonusType != BonusType::MOVIE || env.getQuestId() != questId)
			return HandlerResult::UNKNOWN;

		runtime::Ptr<Player> player = env.getPlayer();
		runtime::Ptr<QuestState> qs = player->getQuestStateList()->getQuestState(questId);
		if (qs != nullptr && qs->getStatus() == QuestStatus::REWARD) {
			if (qs->getCompleteCount() == 9) { // [Event] Hat Box
				rewardItems.push_back(QuestItems(188051106, 1));
			}
			// randomize movie
			playQuestMovie(env, ::aion::commons::utils::Rnd::nextBoolean() ? 103 : 104);
			return HandlerResult::SUCCESS;
		}
		return HandlerResult::FAILED;
	}
};
AION_QUEST_HANDLER(_80016EventSockHop, 80016);

} // namespace aion::gameserver::handlers::quest::event_quests
