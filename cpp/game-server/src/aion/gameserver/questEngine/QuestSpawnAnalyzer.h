#pragma once

#include <cstdint>
#include <unordered_set>
#include <vector>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/templates/quest/fwd.h"
#include "aion/gameserver/questEngine/fwd.h"
#include "aion/gameserver/questEngine/handlers/fwd.h"

namespace aion::gameserver::questEngine {

/**
 * Java com.aionemu.gameserver.questEngine.QuestSpawnAnalyzer: finds the npcs that quests are registered at although nothing spawns them. With
 * gameserver.analysis.quest_handlers on, QuestEngine::init runs it once on the long-running pool; it logs the result and changes nothing.
 * <p>
 * The npc ids the handlers spawn (loadNpcIdsSpawnedByHandlers): Java compiles the handler sources at startup, so it reads them there too and
 * runs the pattern <tt>\bsp(?:awn)?\([^,\d]*(\d{6})(?: : (\d{6}))?</tt> over every .java file below the instance, quest and ai handler
 * directories (InstanceConfig.HANDLER_DIRECTORY, GSConfig.QUEST_HANDLER_DIRECTORY, AIConfig.HANDLER_DIRECTORY). The C++ handlers are compiled
 * in and the server has no sources at runtime, so the tool aion_gs_regscan replaces parseSpawnNpcIds at build time: it runs the same pattern
 * over the raw text of the C++ handler sources of the same three directories (handlers/aion/gameserver/handlers/{ai,instance,quest}, .cpp and
 * .h; tools/regscan/src/SpawnIds.cpp) and emits the sorted set as HandlerRegistry.h's npcIdsSpawnedByHandlers(), which
 * loadNpcIdsSpawnedByHandlers copies (handlers-and-porting-plan.md §1.9, docs/DEVIATIONS.md "QuestSpawnAnalyzer"). The set therefore holds the
 * ids of the ported handlers, exactly as Java's holds the ids of the handlers it loads; the handler porting convention keeps npc ids of
 * spawn(/sp( calls literal (CONVENTIONS.md). parseSpawnNpcIds(File, Pattern, Set) is not ported: that scan has no C++ counterpart at runtime.
 * <p>
 * Everything else is Java's: the spawned npc ids (the handlers', SpawnsData, TownSpawnsData, EventData), the factions with a spawned npc, the
 * unobtainable quests (minlevel_permitted 99, a faction without a spawned npc, or a start condition whose finished quests are all unobtainable)
 * and, per unspawned quest npc, its registered quests that are not unobtainable and have no spawned alternative npc (XMLQuest.getAlternativeNpcs).
 * The log lines are Java's, their order included (sorted npc ids, sorted quest ids, sorted lines).
 */
class QuestSpawnAnalyzer final {
private:
	QuestSpawnAnalyzer() = default;

public:
	/**
	 * Java: static void run(Collection<AbstractQuestHandler>, Collection<QuestNpc>, boolean). QuestEngine::init passes snapshots of its
	 * questHandlers and questNpcs values (Java: the live views of the maps, read on the pool thread; the engine does not change them after init).
	 */
	static void run(const std::vector<handlers::AbstractQuestHandler*>& questHandlers,
		const std::vector<runtime::Ptr<gameserver::model::templates::quest::QuestNpc>>& questNpcs, bool ignoreEventQuests);

private:
	static bool isUnobtainable(int32_t questId, const std::unordered_set<int32_t>& unobtainableQuests);

	/** @return True, if alternative npc ids, which are valid for this quest, appear in spawn templates (e.g. mobs for quest kills or talk npcs) */
	static bool existsSpawnDataForAnyAlternativeNpc(int32_t questId, int32_t npcId, const std::unordered_set<int32_t>& allSpawns);

public:
	/** @return the npc ids the compiled-in instance, quest and ai handlers spawn (the build-time scan of the class comment) */
	static std::unordered_set<int32_t> loadNpcIdsSpawnedByHandlers();
};

} // namespace aion::gameserver::questEngine
