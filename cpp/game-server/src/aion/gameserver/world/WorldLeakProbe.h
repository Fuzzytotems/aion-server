#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/runtime/services/LeakCensus.h"

namespace aion::gameserver::world {

/**
 * C++ only (no Java counterpart; docs/deviations/P4-10.md): names the references that keep an object alive after World.removeObject. The leak
 * census reports such an object with its reference count, but it cannot say whose the references are - the leak of the 2026-09-24 client
 * session (an Npc at refcount 1, no task, no player) could not be attributed from the log. findHolders makes one pass over the world for all the
 * objects it is given, and logHolders logs one warning per structure that still references one of them, or a warning naming what it searched
 * and what it could not. Neither changes game state; the one write is the walker lookup's (see findHolders).
 * <p>
 * The leak census holder probe of the game layer (runtime::LeakCensus::HolderProbe, header request m5b3-leak-h01): a census check that reports
 * new leaks runs probe() once on the instant pool for at most LeakCensus::MAX_PROBED_LEAKS_PER_PASS of them, pinned on them, at most once a
 * minute, and never for the leaks of a zero-threshold check (CheckOutput's final census, whose counts the pin would hold up).
 * <p>
 * Searched, by identity (SEARCHED): World.allObjects; for every object in the world its target and known list, for every creature its aggro
 * list, its casting skill (effector, first target, effected list) and its effects (effector, effected), for every player Player.postman and for
 * every house House.spawns (butler, relationship crystal, sign); in every instance of every world map worldMapObjects, worldMapNpcs, the object
 * map of every map region and the creature map of every zone; for a walker npc the WalkerGroup of its route in its instance
 * (WalkerFormationsCache -> InstanceWalkerFormations.walkFormations) and the group of its own Npc.walkerGroup (WalkerGroup.members ->
 * ClusteredNpc.npc); and SiegeLocation.creatures of every siege location. Not searched (NOT_SEARCHED): the structures the port keeps private
 * with no reader (header request m5b3-leak-h03 in docs/porting/header-requests.md), the other fields of world objects, objects outside the
 * world, the creatures' observers (an observer's captures cannot be searched by identity), the tasks (the census names the pinning tasks) and
 * the other service singletons.
 */
class WorldLeakProbe final {
public:
	/** the structures findHolders searches, for the "no holder found" warning */
	static const char* const SEARCHED;
	/** the structures findHolders cannot search, for the "no holder found" warning */
	static const char* const NOT_SEARCHED;

	/** One object to look for, with the leak census's name for it: its class (static storage duration) and its object id */
	struct ProbedObject {
		model::gameobjects::VisibleObject* object = nullptr;
		const char* className = nullptr;
		int32_t objectId = 0;
	};

	/**
	 * For each of `objects` (same order), one description per structure that references it; empty if none does. One pass over the world for
	 * all of them, taking each world object's snapshots (aggro list, casting skill, effects) once. Runs in a task scope. The walker lookup is
	 * WalkerFormationsCache's only one, a computeIfAbsent: for a walker npc whose instance's formations WalkerFormator.onInstanceDestroy already
	 * dropped it creates an empty InstanceWalkerFormations entry (processClusteredNpc created the entry of every other walker's instance when it
	 * spawned the npc).
	 */
	static std::vector<std::vector<std::string>> findHolders(std::span<model::gameobjects::VisibleObject* const> objects);

	/** findHolders of one object */
	static std::vector<std::string> findHolders(model::gameobjects::VisibleObject& object);

	/**
	 * Logs what findHolders finds for `objects`, in one pass: `Leak probe: <class> (object id N, <object>) is held by <structure>` per holder,
	 * or `Leak probe: <class> (object id N, <object>): no holder found. Searched by identity: <SEARCHED>. Not searched: <NOT_SEARCHED>`. The
	 * caller keeps the objects alive while it runs. Runs in a task scope.
	 */
	static void logHolders(std::span<const ProbedObject> objects);

	/** runtime::LeakCensus::HolderProbe: logHolders for the visible objects among `leaks` */
	static void probe(std::span<const runtime::LeakCensus::ProbedLeak> leaks);

	/** Sets probe() as the leak census holder probe (GameServer's World step). Idempotent. */
	static void install() noexcept;

	WorldLeakProbe() = delete;
};

} // namespace aion::gameserver::world
