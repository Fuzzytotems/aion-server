#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/questEngine/handlers/HandlerResult.h"
#include "aion/gameserver/questEngine/handlers/models/fwd.h"
#include "aion/gameserver/questEngine/handlers/template/AbstractTemplateQuestHandler.h"
#include "aion/gameserver/questEngine/model/fwd.h"
#include "aion/gameserver/runtime/collections/ArrayList.h"
#include "aion/gameserver/runtime/collections/HashSet.h"
#include "aion/gameserver/runtime/fields/Field.h"

namespace aion::gameserver::questEngine::handlers::template_ {

/**
 * Java com.aionemu.gameserver.questEngine.handlers.template.ReportToMany: the `report_to_many` XML quests - one step per <npc_infos>, the last
 * npc reports; started at a start npc or, with `start_item_id`, by using that item.
 * <p>
 * C++ differences:
 * - A Java `List<Integer>` parameter that may be null is `std::optional<std::vector<int32_t>>` (nullopt for null).
 * - `npcInfos` points to the NpcInfos of the ReportToManyData that built the handler (static data, alive for the process).
 * - `rewardStatusFromRewardNpc` is one flag of the handler, not of a player, as in Java (a SET_SUCCEED of any player clears it for good).
 *
 * @author Hilgert, vlog, Pad, Neon
 */
class ReportToMany : public AbstractTemplateQuestHandler {
private:
	const int32_t startItemId;
	runtime::HashSet<int32_t> startNpcIds{AION_LOCK_CLASS(ReportToMany::startNpcIds)};
	const int32_t startDialogId;
	runtime::ArrayList<const models::NpcInfos*> npcInfos{AION_LOCK_CLASS(ReportToMany::npcInfos)};
	const bool mission;
	const bool isDataDriven;
	runtime::Field<bool> rewardStatusFromRewardNpc{true}; // workaround flag for end npc dialog behavior (see below)

public:
	ReportToMany(int32_t questId, int32_t startItemId, const std::optional<std::vector<int32_t>>& startNpcIds,
		const std::vector<models::NpcInfos>& npcInfos, int32_t startDialogId, bool mission);

	void register_() override;

	bool onDialogEvent(model::QuestEnv& env) override;

private:
	int32_t getMaxStep() const;

	int32_t getDialogId(int32_t var) const;

	bool validateAndRemoveItems(model::QuestEnv& env);

public:
	HandlerResult onItemUseEvent(model::QuestEnv& env, gameserver::model::gameobjects::Item& item) override;

	void onLevelChangedEvent(gameserver::model::gameobjects::player::Player& player) override;
};

} // namespace aion::gameserver::questEngine::handlers::template_
