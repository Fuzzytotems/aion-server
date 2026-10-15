#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include "aion/gameserver/questEngine/handlers/template/AbstractTemplateQuestHandler.h"
#include "aion/gameserver/questEngine/model/fwd.h"
#include "aion/gameserver/runtime/collections/HashSet.h"

namespace aion::gameserver::questEngine::handlers::template_ {

/**
 * Java com.aionemu.gameserver.questEngine.handlers.template.RelicRewards: the `relic_rewards` XML quests - relics the player owns exchanged
 * at an npc (EXCHANGE_COIN), the reward group chosen by which of the four collect-item categories the player can hand in.
 * <p>
 * C++: a Java `List<Integer>` parameter that may be null is `std::optional<std::vector<int32_t>>` (nullopt for null). `isDataDriven` is
 * effectively final in Java (assigned once by the constructor), so it is const here.
 *
 * @author Bobobear, Rolandas, Pad
 */
class RelicRewards : public AbstractTemplateQuestHandler {
private:
	runtime::HashSet<int32_t> startNpcIds{AION_LOCK_CLASS(RelicRewards::startNpcIds)};
	const bool isDataDriven;

public:
	RelicRewards(int32_t questId, const std::optional<std::vector<int32_t>>& startNpcIds);

	void register_() override;

	bool onDialogEvent(model::QuestEnv& env) override;
};

} // namespace aion::gameserver::questEngine::handlers::template_
