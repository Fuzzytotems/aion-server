#include "aion/gameserver/handlers/quest/QuestPrelude.h"

#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"
#include "aion/gameserver/model/gameobjects/player/QuestStateList.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/model/templates/quest/QuestNpc.h"
#include "aion/gameserver/services/event/EventService.h"
#include "aion/gameserver/services/QuestService.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"

namespace aion::gameserver::handlers::quest::event_quests {

// Hand-ported from game-server/data/handlers/quest/event_quests/_80030EventAnUnwelcomeGaze.java (questgen refuses it: an anonymous `new Runnable() {...}`,
// EventService.getInstance and Race.equals), in the questgen output's style: drafted by transliterating a copy of the Java whose Runnable
// is the equivalent lambda (rule scheduled-closure) and whose `getRace().equals(Race.X)` is `==` (an enum), then the event check written by
// hand. No correction of the Java code (lane C, 2026-10-15; docs/deviations/Q12.md, "80030 and 80033").

/**
 * @author Rolandas
 */
class _80030EventAnUnwelcomeGaze final : public AbstractQuestHandler {
public:
	_80030EventAnUnwelcomeGaze() : AbstractQuestHandler(80030) {}

	void register_() override {
		qe.registerQuestNpc(799766)->addOnQuestStart(questId);
		qe.registerQuestNpc(799766)->addOnTalkEvent(questId);
		qe.registerQuestItem(188051133, questId); // [Event] Charm Card
	}

	bool onDialogEvent(QuestEnv& env) override {
		runtime::Ptr<Player> player = env.getPlayer();
		int32_t targetId = env.getTargetId();
		runtime::Ptr<QuestState> qs = player->getQuestStateList()->getQuestState(questId);

		if (qs == nullptr || qs->isStartable())
			return false;

		if (qs->getStatus() == QuestStatus::START) {
			if (targetId == 799766) {
				if (env.getDialogActionId() == QUEST_SELECT)
					return sendQuestDialog(env, 1011);
				else if (env.getDialogActionId() == QUEST_ACCEPT_1) {
					Storage& storage = player->getInventory();
					if (storage.getItemCountByItemId(164002015) > 0)
						return sendQuestDialog(env, 2375);
					else
						return sendQuestDialog(env, 2716);
				} else if (env.getDialogActionId() == SELECT_QUEST_REWARD) {
					if (qs->getQuestVarById(0) == 0)
						defaultCloseDialog(env, 0, 1, true, true, 0, 0, 164002015, 1);
					return sendQuestDialog(env, 5);
				} else if (env.getDialogActionId() == SELECTED_QUEST_NOREWARD)
					return sendQuestRewardDialog(env, 799766, 5);
				else
					return sendQuestStartDialog(env);
			}
		}
		return sendQuestRewardDialog(env, 799766, 0);
	}

	HandlerResult onItemUseEvent(QuestEnv& env, Item& item) override {
		// check if the parent quest is active (you get Charm Cards)
		if (!services::event::EventService::getInstance().isActiveEventQuest(80029)) // check if the parent quest is active
			return HandlerResult::UNKNOWN;

		runtime::Ptr<Player> player = env.getPlayer();

		if (item.getItemId() == 188051133 && player->getCommonData()->getRace() == Race::ELYOS) {
			ThreadPoolManager::getInstance().schedule({this}, [this, player = runtime::Ref<Player>(player)] {
				Storage& storage = player->getInventory();
				QuestStatus status;

				if (storage.getItemCountByItemId(164002015) > 0) {
					status = getQuestUpdateStatus(player, questId);
					// got a Beritra's Gaze, then start me
					QuestService::startEventQuest(*QuestEnv::create(nullptr, *player, questId), status);
				}
				if (storage.getItemCountByItemId(164002016) > 9) { // Israphel's Glory
					status = getQuestUpdateStatus(player, 80034);
					QuestService::startEventQuest(*QuestEnv::create(nullptr, *player, 80034), status);
				}
				if (storage.getItemCountByItemId(164002017) > 4) { // Siel's Gift
					status = getQuestUpdateStatus(player, 80035);
					QuestService::startEventQuest(*QuestEnv::create(nullptr, *player, 80035), status);
				}
				if (storage.getItemCountByItemId(164002018) > 0) { // Aion's Grace
					status = getQuestUpdateStatus(player, 80036);
					QuestService::startEventQuest(*QuestEnv::create(nullptr, *player, 80036), status);
				}
			}, 10000);
			return HandlerResult::SUCCESS;
		}
		return HandlerResult::UNKNOWN;
	}

private:
	QuestStatus getQuestUpdateStatus(runtime::Ptr<Player> player, int32_t questid) {
		runtime::Ptr<QuestState> qs = player->getQuestStateList()->getQuestState(questid);
		QuestStatus status = qs == nullptr ? QuestStatus::START : qs->getStatus();

		if (qs != nullptr && questid != questId && status == QuestStatus::COMPLETE)
			status = QuestStatus::START;
		return status;
	}
};
AION_QUEST_HANDLER(_80030EventAnUnwelcomeGaze, 80030);

} // namespace aion::gameserver::handlers::quest::event_quests
