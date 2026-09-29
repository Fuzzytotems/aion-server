#pragma once

#include <cstdint>

#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/questEngine/handlers/HandlerResult.h"
#include "aion/gameserver/questEngine/handlers/template/AbstractTemplateQuestHandler.h"
#include "aion/gameserver/questEngine/model/fwd.h"

namespace aion::gameserver::questEngine::handlers::template_ {

/**
 * Java com.aionemu.gameserver.questEngine.handlers.template.ItemOrders: the `item_order` XML quests - started by using the quest's work item
 * (the start item), talked through at one or two npcs and reported at an end npc.
 * <p>
 * C++: `startItemId` is effectively final in Java (assigned once by the constructor, 0 without a work item), so it is const here.
 *
 * @author Altaress, Bobobear, Pad
 */
class ItemOrders : public AbstractTemplateQuestHandler {
private:
	const int32_t startItemId;
	const int32_t talkNpcId1;
	const int32_t talkNpcId2;
	const int32_t endNpcId;

public:
	ItemOrders(int32_t questId, int32_t talkNpcId1, int32_t talkNpcId2, int32_t endNpcId);

	void register_() override;

	bool onDialogEvent(model::QuestEnv& env) override;

	HandlerResult onItemUseEvent(model::QuestEnv& env, gameserver::model::gameobjects::Item& item) override;

private:
	/** C++ only: the constructor's work-item block (ItemOrders.java:38-43), the start item id it assigns (0 when it assigns none) */
	int32_t startItemIdOfWorkItems();
};

} // namespace aion::gameserver::questEngine::handlers::template_
