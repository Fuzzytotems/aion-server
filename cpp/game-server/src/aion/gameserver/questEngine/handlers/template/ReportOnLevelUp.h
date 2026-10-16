#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/questEngine/handlers/template/AbstractTemplateQuestHandler.h"
#include "aion/gameserver/questEngine/model/fwd.h"
#include "aion/gameserver/runtime/collections/HashSet.h"

namespace aion::gameserver::questEngine::handlers::template_ {

/**
 * Java com.aionemu.gameserver.questEngine.handlers.template.ReportOnLevelUp: the `report_on_levelup` XML quests (the ten stigma quests) -
 * started in REWARD at the first enter world or level change that passes the start conditions, reported at an end npc.
 * <p>
 * C++: `endNpcIds` null is std::nullopt.
 *
 * @author Majka, Bobobear, Pad
 */
class ReportOnLevelUp : public AbstractTemplateQuestHandler {
private:
	runtime::HashSet<int32_t> endNpcIds{AION_LOCK_CLASS(ReportOnLevelUp::endNpcIds)};

public:
	ReportOnLevelUp(int32_t questId, const std::optional<std::vector<int32_t>>& endNpcIds);

	void register_() override;

	bool onDialogEvent(model::QuestEnv& env) override;

	bool onEnterWorldEvent(model::QuestEnv& env) override;

	void onLevelChangedEvent(gameserver::model::gameobjects::player::Player& player) override;

private:
	bool startQuest(gameserver::model::gameobjects::player::Player& player);
};

} // namespace aion::gameserver::questEngine::handlers::template_
