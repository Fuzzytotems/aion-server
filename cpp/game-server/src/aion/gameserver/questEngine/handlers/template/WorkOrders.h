#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include "aion/gameserver/model/templates/quest/fwd.h"
#include "aion/gameserver/questEngine/handlers/template/AbstractTemplateQuestHandler.h"
#include "aion/gameserver/questEngine/model/fwd.h"
#include "aion/gameserver/runtime/collections/ArrayList.h"
#include "aion/gameserver/runtime/collections/HashSet.h"

namespace aion::gameserver::questEngine::handlers::template_ {

/**
 * Java com.aionemu.gameserver.questEngine.handlers.template.WorkOrders: the `work_order` XML quests - a crafting task: the accept gives the
 * components and teaches the recipe, the report takes the crafted items, the reward deletes the recipe again.
 * <p>
 * C++ differences:
 * - `startNpcIds` (required) arrives as the bound `std::optional<std::vector<int32_t>>`; Java's `addAll(null)` NullPointerException for an
 *   absent attribute is thrown the same way.
 * - `giveComponents` points to the <give_component> elements of the WorkOrdersData that built the handler (Java: the same objects; static
 *   data, alive for the process). Java's `addAll(null)` for a quest without them has no C++ counterpart: an absent list binds as an empty
 *   vector (all 574 work orders of the data have components).
 *
 * @author Mr. Poke, Bobobear, Pad
 */
class WorkOrders : public AbstractTemplateQuestHandler {
private:
	runtime::HashSet<int32_t> startNpcIds{AION_LOCK_CLASS(WorkOrders::startNpcIds)};
	runtime::ArrayList<const gameserver::model::templates::quest::QuestItems*> giveComponents{AION_LOCK_CLASS(WorkOrders::giveComponents)};
	const int32_t recipeId;

public:
	WorkOrders(int32_t questId, const std::optional<std::vector<int32_t>>& startNpcIds,
		const std::vector<gameserver::model::templates::quest::QuestItems>& giveComponents, int32_t recipeId);

	void register_() override;

	bool onDialogEvent(model::QuestEnv& env) override;
};

} // namespace aion::gameserver::questEngine::handlers::template_
