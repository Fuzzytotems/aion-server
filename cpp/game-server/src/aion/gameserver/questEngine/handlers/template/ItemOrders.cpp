#include "aion/gameserver/questEngine/handlers/template/ItemOrders.h"

#include <string>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/ItemData.h"
#include "aion/gameserver/model/DialogAction.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/QuestStateList.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/model/templates/quest/QuestItems.h"
#include "aion/gameserver/model/templates/quest/QuestNpc.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/questEngine/QuestEngine.h"
#include "aion/gameserver/questEngine/handlers/HandlerResultInfo.h"
#include "aion/gameserver/questEngine/model/QuestEnv.h"
#include "aion/gameserver/questEngine/model/QuestState.h"
#include "aion/gameserver/questEngine/model/QuestStatus.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/services/QuestService.h"
#include "aion/gameserver/utils/PacketSendUtility.h"

namespace aion::gameserver::questEngine::handlers::template_ {

namespace DialogAction = gameserver::model::DialogAction;
using gameserver::model::gameobjects::player::Player;
using model::QuestState;
using model::QuestStatus;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.questEngine.handlers.template.ItemOrders");

ItemOrders::ItemOrders(int32_t questIdValue, int32_t talkNpcId1Value, int32_t talkNpcId2Value, int32_t endNpcIdValue)
	: AbstractTemplateQuestHandler(questIdValue), startItemId(startItemIdOfWorkItems()), talkNpcId1(talkNpcId1Value),
	  talkNpcId2(talkNpcId2Value), endNpcId(endNpcIdValue) {
}

int32_t ItemOrders::startItemIdOfWorkItems() {
	runtime::Ptr<runtime::RcArrayList<const gameserver::model::templates::quest::QuestItems*>> items = workItems.get();
	if (!items) {
		log.warn("Q{} has no work item", questId);
		return 0;
	}
	if (items->size() > 1)
		log.warn("Q{} has more than 1 work item", questId);
	return items->get(0)->getItemId();
}

void ItemOrders::register_() {
	qe.registerQuestItem(startItemId, questId);
	if (talkNpcId1 != 0)
		qe.registerQuestNpc(talkNpcId1)->addOnTalkEvent(questId);
	if (talkNpcId2 != 0)
		qe.registerQuestNpc(talkNpcId2)->addOnTalkEvent(questId);
	if (endNpcId != 0)
		qe.registerQuestNpc(endNpcId)->addOnTalkEvent(questId);
}

bool ItemOrders::onDialogEvent(model::QuestEnv& env) {
	runtime::Ptr<Player> player = env.getPlayer();
	runtime::Ptr<QuestState> qs = player->getQuestStateList()->getQuestState(questId);
	int32_t dialogActionId = env.getDialogActionId();
	int32_t targetId = env.getTargetId();

	if (!qs || qs->isStartable()) {
		switch (dialogActionId) {
			case DialogAction::QUEST_ACCEPT:
			case DialogAction::QUEST_ACCEPT_1:
			case DialogAction::QUEST_ACCEPT_SIMPLE:
				if (player->getInventory().getItemCountByItemId(startItemId) > 0) {
					services::QuestService::startQuest(env);
				} else {
					const gameserver::model::templates::item::ItemTemplate* startItem =
						dataholders::DataManager::ITEM_DATA->getItemTemplate(startItemId);
					if (startItem == nullptr) // Java: NullPointerException on getL10n()
						throw runtime::NullPointerException("ITEM_DATA.getItemTemplate(" + std::to_string(startItemId) + ")");
					std::string requiredItemL10n = startItem->getL10n();
					utils::PacketSendUtility::sendPacket(*player, SM_SYSTEM_MESSAGE::STR_QUEST_ACQUIRE_ERROR_INVENTORY_ITEM(requiredItemL10n));
				}
				return closeDialogWindow(env);
			default:
				return AbstractQuestHandler::onDialogEvent(env);
		}
	} else if (qs->getStatus() == QuestStatus::START) {
		int32_t var0 = qs->getQuestVarById(0);
		if (targetId == talkNpcId1 || targetId == talkNpcId2) {
			if (dialogActionId == DialogAction::QUEST_SELECT) {
				return sendQuestDialog(env, 1352);
			} else if (dialogActionId == DialogAction::SETPRO1) {
				bool reward = ((var0 == 0 && talkNpcId2 == 0) || (var0 == 1 && talkNpcId2 != 0));
				qs->setQuestVarById(0, var0 + 1);
				if (reward)
					qs->setStatus(QuestStatus::REWARD);
				updateQuestStatus(env);
				return closeDialogWindow(env);
			}
		} else if (targetId == endNpcId) {
			if (dialogActionId == DialogAction::QUEST_SELECT) {
				return sendQuestDialog(env, 2375);
			} else if (dialogActionId == DialogAction::SELECT_QUEST_REWARD) {
				return defaultCloseDialog(env, 0, 1, true, true);
			}
		}
	} else if (qs->getStatus() == QuestStatus::REWARD) {
		if (targetId == endNpcId) {
			switch (dialogActionId) {
				case DialogAction::USE_OBJECT:
					return sendQuestDialog(env, 2375);
				default: {
					return sendQuestEndDialog(env);
				}
			}
		}
	}
	return false;
}

HandlerResult ItemOrders::onItemUseEvent(model::QuestEnv& env, gameserver::model::gameobjects::Item& /*item*/) {
	runtime::Ptr<Player> player = env.getPlayer();
	runtime::Ptr<QuestState> qs = player->getQuestStateList()->getQuestState(questId);
	if (!qs || qs->isStartable()) {
		return fromBoolean(sendQuestDialog(env, 4));
	}
	return HandlerResult::FAILED;
}

} // namespace aion::gameserver::questEngine::handlers::template_
