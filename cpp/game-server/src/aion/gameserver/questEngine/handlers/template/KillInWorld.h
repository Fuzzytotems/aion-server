#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include "aion/gameserver/questEngine/handlers/template/AbstractTemplateQuestHandler.h"
#include "aion/gameserver/questEngine/model/fwd.h"
#include "aion/gameserver/runtime/collections/HashSet.h"

namespace aion::gameserver::questEngine::handlers::template_ {

/**
 * Java com.aionemu.gameserver.questEngine.handlers.template.KillInWorld: standard xml-based handling for the DAILY quests with
 * onKillInWorld events (the `kill_in_world` XML quests: player kills in the listed worlds, counted by PvpService).
 * <p>
 * C++: a Java `List<Integer>` parameter that may be null is `std::optional<std::vector<int32_t>>` (nullopt for null).
 *
 * @author vlog, bobobear, Pad
 */
class KillInWorld : public AbstractTemplateQuestHandler {
private:
	runtime::HashSet<int32_t> startNpcIds{AION_LOCK_CLASS(KillInWorld::startNpcIds)};
	runtime::HashSet<int32_t> endNpcIds{AION_LOCK_CLASS(KillInWorld::endNpcIds)};
	runtime::HashSet<int32_t> worldIds{AION_LOCK_CLASS(KillInWorld::worldIds)};
	const int32_t killAmount;
	const int32_t minRank;
	const int32_t levelDiff;
	const int32_t invasionWorldId;
	const int32_t startDialogId;
	const int32_t startDistanceNpcId;
	const int32_t endDialogId;
	const bool isDataDriven;

public:
	KillInWorld(int32_t questId, const std::optional<std::vector<int32_t>>& endNpcIds, const std::optional<std::vector<int32_t>>& startNpcIds,
		const std::optional<std::vector<int32_t>>& worldIds, int32_t killAmount, int32_t minRank, int32_t levelDiff, int32_t invasionWorld,
		int32_t startDialogId, int32_t startDistanceNpcId, int32_t endDialogId);

	void register_() override;

	bool onDialogEvent(model::QuestEnv& env) override;

	bool onEnterWorldEvent(model::QuestEnv& env) override;

private:
	bool searchOpenRift();

public:
	bool onKillInWorldEvent(model::QuestEnv& env) override;

	bool onAtDistanceEvent(model::QuestEnv& env) override;
};

} // namespace aion::gameserver::questEngine::handlers::template_
