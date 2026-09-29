#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include "aion/gameserver/questEngine/handlers/models/fwd.h"
#include "aion/gameserver/questEngine/handlers/template/AbstractTemplateQuestHandler.h"
#include "aion/gameserver/questEngine/model/fwd.h"
#include "aion/gameserver/runtime/collections/ArrayList.h"
#include "aion/gameserver/runtime/collections/HashSet.h"

namespace aion::gameserver::questEngine::handlers::template_ {

/**
 * Java com.aionemu.gameserver.questEngine.handlers.template.KillSpawned: the `kill_spawned` XML quests - a quest object (the spawner) spawns
 * a monster for five minutes, whose kills count in the quest vars.
 * <p>
 * C++ differences:
 * - A Java `List<Integer>` parameter that may be null is `std::optional<std::vector<int32_t>>` (nullopt for null).
 * - `spawnedMonsters` points to the Monster elements of the KillSpawnedData that built the handler (Java: the same objects; static data,
 *   alive for the process). An absent <monster> list is the bound empty vector, which Java's `Collections.emptyList()` also gives.
 *
 * @author vlog, Pad
 */
class KillSpawned : public AbstractTemplateQuestHandler {
private:
	runtime::HashSet<int32_t> startNpcIds{AION_LOCK_CLASS(KillSpawned::startNpcIds)};
	runtime::HashSet<int32_t> endNpcIds{AION_LOCK_CLASS(KillSpawned::endNpcIds)};
	runtime::HashSet<int32_t> spawnerObjectIds{AION_LOCK_CLASS(KillSpawned::spawnerObjectIds)};
	runtime::ArrayList<const models::Monster*> spawnedMonsters{AION_LOCK_CLASS(KillSpawned::spawnedMonsters)};
	const bool isDataDriven;

public:
	KillSpawned(int32_t questId, const std::optional<std::vector<int32_t>>& startNpcIds, const std::optional<std::vector<int32_t>>& endNpcIds,
		const std::vector<models::Monster>& spawnedMonsters);

	void register_() override;

	bool onDialogEvent(model::QuestEnv& env) override;

	bool onKillEvent(model::QuestEnv& env) override;
};

} // namespace aion::gameserver::questEngine::handlers::template_
