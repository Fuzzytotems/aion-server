#include "aion/gameserver/questEngine/handlers/template/WorkOrders.h"

#include <string>

#include "aion/gameserver/model/DialogAction.h"
#include "aion/gameserver/model/DialogPageInfo.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/QuestStateList.h"
#include "aion/gameserver/model/gameobjects/player/RecipeList.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/model/templates/QuestTemplate.h"
#include "aion/gameserver/model/templates/quest/CollectItem.h"
#include "aion/gameserver/model/templates/quest/CollectItems.h"
#include "aion/gameserver/model/templates/quest/QuestItems.h"
#include "aion/gameserver/model/templates/quest/QuestNpc.h"
#include "aion/gameserver/questEngine/QuestEngine.h"
#include "aion/gameserver/questEngine/model/QuestEnv.h"
#include "aion/gameserver/questEngine/model/QuestState.h"
#include "aion/gameserver/questEngine/model/QuestStatus.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/services/QuestService.h"
#include "aion/gameserver/services/RecipeService.h"
#include "aion/gameserver/services/item/ItemService.h"

namespace aion::gameserver::questEngine::handlers::template_ {

namespace DialogAction = gameserver::model::DialogAction;
using gameserver::model::DialogPage;
using gameserver::model::gameobjects::player::Player;
using gameserver::model::templates::quest::QuestItems;
using model::QuestState;
using model::QuestStatus;
using services::QuestService;

WorkOrders::WorkOrders(int32_t questIdValue, const std::optional<std::vector<int32_t>>& startNpcIdsValue,
	const std::vector<QuestItems>& giveComponentsValue, int32_t recipeIdValue)
	: AbstractTemplateQuestHandler(questIdValue), recipeId(recipeIdValue) {
	if (!startNpcIdsValue) // Java: this.startNpcIds.addAll(null)
		throw runtime::NullPointerException("WorkOrders " + std::to_string(questIdValue) + ": startNpcIds");
	startNpcIds.addAll(*startNpcIdsValue);
	for (const QuestItems& component : giveComponentsValue)
		giveComponents.add(&component);
}

void WorkOrders::register_() {
	for (int32_t startNpcId : startNpcIds) {
		qe.registerQuestNpc(startNpcId)->addOnQuestStart(questId);
		qe.registerQuestNpc(startNpcId)->addOnTalkEvent(questId);
	}
}

bool WorkOrders::onDialogEvent(model::QuestEnv& env) {
	runtime::Ptr<Player> player = env.getPlayer();
	runtime::Ptr<QuestState> qs = player->getQuestStateList()->getQuestState(questId);
	int32_t dialogActionId = env.getDialogActionId();
	int32_t targetId = env.getTargetId();

	if (startNpcIds.contains(targetId)) {
		if (!qs || qs->isStartable()) {
			switch (dialogActionId) {
				case DialogAction::QUEST_SELECT:
					return sendQuestDialog(env, gameserver::model::id(DialogPage::ASK_QUEST_ACCEPT_WINDOW));
				case DialogAction::QUEST_ACCEPT_1:
					if (services::RecipeService::validateNewRecipe(*player, recipeId) != nullptr) {
						if (QuestService::startQuest(env)) {
							for (const QuestItems* qi : giveComponents.snapshot())
								services::item::ItemService::addItem(*player, qi->getItemId(), qi->getCount(), true);
							services::RecipeService::addRecipe(*player, recipeId, false);
							closeDialogWindow(env);
							return true;
						}
					}
					return false;
				case DialogAction::COMBINE_TASK:
					env.setQuestId(0);
					return sendQuestDialog(env, gameserver::model::id(DialogPage::COMBINETASK_WINDOW));
			}
		} else if (qs->getStatus() == QuestStatus::START) {
			if (dialogActionId == DialogAction::QUEST_SELECT) {
				int32_t var = qs->getQuestVarById(0);
				if (QuestService::collectItemCheck(env, false)) {
					changeQuestStep(env, var, var, true); // reward
					QuestService::removeQuestWorkItems(*player, *qs);
					return sendQuestDialog(env, gameserver::model::id(DialogPage::SELECT_QUEST_REWARD_WINDOW1));
				} else {
					return sendQuestSelectionDialog(env);
				}
			}
		} else if (qs->getStatus() == QuestStatus::REWARD) {
			const gameserver::model::templates::quest::CollectItems* collectItems = questTemplateOf(questId).getCollectItems();
			if (collectItems == nullptr) // Java: NullPointerException on getCollectItem()
				throw runtime::NullPointerException("quest " + std::to_string(questId) + " has no collect_items");
			int64_t count = 0;
			for (const gameserver::model::templates::quest::CollectItem& collectItem : collectItems->getCollectItem()) {
				if (!collectItem.getItemId()) // Java: the Integer item id unboxed
					throw runtime::NullPointerException("collect_item without item_id in quest " + std::to_string(questId));
				count = player->getInventory().getItemCountByItemId(*collectItem.getItemId());
				if (count > 0)
					player->getInventory().decreaseByItemId(*collectItem.getItemId(), count);
			}
			player->getRecipeList()->deleteRecipe(*player, recipeId);
			if (dialogActionId == DialogAction::USE_OBJECT) {
				QuestService::finishQuest(env);
				env.setQuestId(questId);
				return sendQuestDialog(env, 1008);
			} else {
				return sendQuestEndDialog(env);
			}
		}
	}
	return false;
}

} // namespace aion::gameserver::questEngine::handlers::template_
