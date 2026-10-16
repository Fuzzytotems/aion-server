#pragma once

#include <cstdint>

#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/questEngine/handlers/template/AbstractTemplateQuestHandler.h"
#include "aion/gameserver/questEngine/model/fwd.h"

namespace aion::gameserver::questEngine::handlers::template_ {

/**
 * Java com.aionemu.gameserver.questEngine.handlers.template.CraftingRewards: the `crafting_rewards` XML quests - accepted and reported at an
 * npc that teaches the next grade of a crafting skill (level 400 expert, 500 master), which the report adds to the player's skill list.
 *
 * @author Bobobear, Pad
 */
class CraftingRewards : public AbstractTemplateQuestHandler {
private:
	const int32_t startNpcId;
	const int32_t endNpcId;
	const int32_t skillId;
	const int32_t levelReward;
	const int32_t questMovie;
	const bool isDataDriven;

public:
	CraftingRewards(int32_t questId, int32_t startNpcId, int32_t skillId, int32_t levelReward, int32_t endNpcId, int32_t questMovie);

	void register_() override;

	bool onDialogEvent(model::QuestEnv& env) override;

private:
	bool canLearn(gameserver::model::gameobjects::player::Player& player);
};

} // namespace aion::gameserver::questEngine::handlers::template_
