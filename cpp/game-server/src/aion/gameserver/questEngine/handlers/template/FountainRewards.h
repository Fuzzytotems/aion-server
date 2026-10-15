#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include "aion/gameserver/questEngine/handlers/template/AbstractTemplateQuestHandler.h"
#include "aion/gameserver/questEngine/model/fwd.h"
#include "aion/gameserver/runtime/collections/HashSet.h"

namespace aion::gameserver::questEngine::handlers::template_ {

/**
 * Java com.aionemu.gameserver.questEngine.handlers.template.FountainRewards: the `fountain_rewards` XML quests - coins handed in at a Coin
 * Fountain, which starts the quest and asks for the reward in one go.
 * <p>
 * C++: a Java `List<Integer>` parameter that may be null is `std::optional<std::vector<int32_t>>` (nullopt for null).
 *
 * @author Wakizashi, vlog, Bobobear, Luzien, Pad
 */
class FountainRewards : public AbstractTemplateQuestHandler {
private:
	runtime::HashSet<int32_t> startNpcIds{AION_LOCK_CLASS(FountainRewards::startNpcIds)};

public:
	FountainRewards(int32_t questId, const std::optional<std::vector<int32_t>>& startNpcIds);

	void register_() override;

	bool onDialogEvent(model::QuestEnv& env) override;
};

} // namespace aion::gameserver::questEngine::handlers::template_
