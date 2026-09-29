#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "aion/gameserver/questEngine/handlers/template/AbstractTemplateQuestHandler.h"
#include "aion/gameserver/questEngine/model/fwd.h"
#include "aion/gameserver/runtime/collections/HashSet.h"

namespace aion::gameserver::questEngine::handlers::template_ {

/**
 * Java com.aionemu.gameserver.questEngine.handlers.template.KillInZone: the `kill_in_zone` XML quests - player kills inside the listed zones,
 * counted by PvpService.
 * <p>
 * C++: a Java `List<Integer>`/`List<String>` parameter that may be null is a std::optional of the vector (nullopt for null).
 *
 * @author Cheatkiller, Majka, Pad
 */
class KillInZone : public AbstractTemplateQuestHandler {
private:
	runtime::HashSet<int32_t> startNpcIds{AION_LOCK_CLASS(KillInZone::startNpcIds)};
	runtime::HashSet<int32_t> endNpcIds{AION_LOCK_CLASS(KillInZone::endNpcIds)};
	runtime::HashSet<std::string> zones{AION_LOCK_CLASS(KillInZone::zones)};
	const int32_t killAmount;
	const int32_t minRank;
	const int32_t levelDiff;
	const int32_t startDistanceNpc;
	const bool isDataDriven;

public:
	KillInZone(int32_t questId, const std::optional<std::vector<int32_t>>& endNpcIds, const std::optional<std::vector<int32_t>>& startNpcIds,
		const std::optional<std::vector<std::string>>& zones, int32_t killAmount, int32_t minRank, int32_t levelDiff, int32_t startDistanceNpc);

	void register_() override;

	bool onDialogEvent(model::QuestEnv& env) override;

	bool onKillInZoneEvent(model::QuestEnv& env) override;

	bool onAtDistanceEvent(model::QuestEnv& env) override;
};

} // namespace aion::gameserver::questEngine::handlers::template_
