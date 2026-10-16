#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include "aion/gameserver/model/templates/quest/fwd.h"
#include "aion/gameserver/questEngine/handlers/template/AbstractTemplateQuestHandler.h"
#include "aion/gameserver/questEngine/model/fwd.h"
#include "aion/gameserver/runtime/collections/HashSet.h"

namespace aion::gameserver::questEngine::handlers::template_ {

/**
 * Java com.aionemu.gameserver.questEngine.handlers.template.ReportTo: the `report_to` XML quests - accepted at a start npc, reported at an
 * end npc.
 * <p>
 * C++: a Java `List<Integer>` parameter that may be null is `std::optional<std::vector<int32_t>>` (nullopt for null), as the xmlgen models bind
 * it. workItem points into the quest template (static data).
 *
 * @author MrPoke, Rolandas, Pad
 */
class ReportTo : public AbstractTemplateQuestHandler {
private:
	runtime::HashSet<int32_t> startNpcIds{AION_LOCK_CLASS(ReportTo::startNpcIds)};
	runtime::HashSet<int32_t> endNpcIds{AION_LOCK_CLASS(ReportTo::endNpcIds)};
	const int32_t startDialogId;
	const bool isDataDriven;
	const gameserver::model::templates::quest::QuestItems* workItem = nullptr;

public:
	ReportTo(int32_t questId, const std::optional<std::vector<int32_t>>& startNpcIds, const std::optional<std::vector<int32_t>>& endNpcIds,
		int32_t startDialogId);

	void register_() override;

	bool onDialogEvent(model::QuestEnv& env) override;
};

} // namespace aion::gameserver::questEngine::handlers::template_
