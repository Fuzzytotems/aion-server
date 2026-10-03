#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "aion/gameserver/model/templates/quest/fwd.h"
#include "aion/gameserver/questEngine/handlers/template/AbstractTemplateQuestHandler.h"
#include "aion/gameserver/questEngine/model/fwd.h"
#include "aion/gameserver/runtime/collections/HashSet.h"
#include "aion/gameserver/world/zone/fwd.h"

namespace aion::gameserver::questEngine::handlers::template_ {

/**
 * Java com.aionemu.gameserver.questEngine.handlers.template.ItemCollecting: the `item_collecting` XML quests - the <collect_items> of the
 * quest template (quest drops of monsters or quest objects, or items from elsewhere) handed in at an end npc.
 * <p>
 * C++: a Java `List<Integer>` parameter that may be null is `std::optional<std::vector<int32_t>>` (nullopt for null); `startZone` is the bound
 * string, "" for Java null (docs/deviations/P5-06c.md). workItem points into the quest template (static data).
 *
 * @author MrPoke, vlog, Rolandas, Majka, Pad
 */
class ItemCollecting : public AbstractTemplateQuestHandler {
private:
	runtime::HashSet<int32_t> startNpcIds{AION_LOCK_CLASS(ItemCollecting::startNpcIds)};
	runtime::HashSet<int32_t> endNpcIds{AION_LOCK_CLASS(ItemCollecting::endNpcIds)};
	const int32_t questMovie;
	const int32_t nextNpcId;
	const int32_t startDialogId;
	const int32_t startDialogId2;
	const int32_t checkOkDialogId;
	const int32_t checkFailDialogId;
	const bool isDataDriven;
	const std::string startZone;
	const gameserver::model::templates::quest::QuestItems* workItem = nullptr;

public:
	ItemCollecting(int32_t questId, const std::optional<std::vector<int32_t>>& startNpcIds, int32_t nextNpcId,
		const std::optional<std::vector<int32_t>>& endNpcIds, std::string startZone, int32_t questMovie, int32_t startDialogId, int32_t startDialogId2,
		int32_t checkOkDialogId, int32_t checkFailDialogId);

	void register_() override;

	bool onDialogEvent(model::QuestEnv& env) override;

	bool onEnterZoneEvent(model::QuestEnv& env, const world::zone::ZoneName* zoneName) override;
};

} // namespace aion::gameserver::questEngine::handlers::template_
