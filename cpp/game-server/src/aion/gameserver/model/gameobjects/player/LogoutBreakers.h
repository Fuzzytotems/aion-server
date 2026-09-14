#pragma once

#include <array>
#include <string_view>
#include <vector>

#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"

namespace aion::gameserver::model::gameobjects::player {

/**
 * C++-only cycle breakers of players and deleted objects (runtime-architecture.md §5.3 "Logout and delete breakers" and "Zombie breaker",
 * RR-2/RR-5/RR-13; conventions-game-server.md "Cycles"). No Java counterpart: Java relies on the garbage collector for these references.
 * <p>
 * Every breaker cuts exactly the references listed in its step table below; the keys are the edge keys of
 * `game-server/generated/concurrency/cycles.toml`, whose resolution names the step (tools/gen/tests/test_fieldmap_real.py checks both tables
 * against that file). Rules for every step:
 * - idempotent, and safe on an object whose Java cleanup already ran;
 * - no notifications (no onTargetChanged, ActionObserver.onRemoved, endEffect, onStatsChange), no packets, no DAO calls;
 * - a step that throws is logged with the step id and the remaining steps still run (the functions are noexcept).
 * <p>
 * Callers
 * - PlayerLeaveWorldService.leaveWorld: `auto breakers = runtime::finally([&] { LogoutBreakers::run(player); });` as its first statement, so
 *   the breakers also run when a DAO call throws (design §14.2f); the enter-world path runs run(player) in its catch block once
 *   PlayerService.getPlayer returned.
 * - VisibleObjectController::onDelete (Java: empty) calls onDelete(getOwner()) as its last statement. Every controller's onDelete reaches it
 *   through super (CreatureController.onDelete: cancelAllTasks, then super), and World.removeObject calls onDelete after despawn.
 * - VisibleObject::breakKnownEdges (runtime::ZombieBreakable, runtime/services/LeakCensus.h) returns breakZombieEdges(*this): the zombie
 *   breaker for objects removed from the world longer than gameserver.runtime.zombie_break_minutes. Every edge it cuts is a warning, i.e. a
 *   breaker that is missing or did not run.
 * <p>
 * Other C++-only breakers that are not object-local live with their Java method and are named in cycles.toml: Skill.removeObservers
 * (firstTargetDieObserver), Effect.endEffect (designatedDispelEffect), GatheringTask (gathererObserver), House.resetRegistry (registry
 * objects), InstanceService.destroyInstance (instance handler, start position, registered team).
 */
class LogoutBreakers final {
public:
	/** A step of run() or onDelete(): its id (cycles.toml texts refer to it), the class whose objects it applies to and the cut edge */
	struct Step {
		std::string_view id;
		std::string_view appliesTo;
		/** cycles.toml edge key; empty for a step that only delegates */
		std::string_view edge;
	};

	/** An edge the zombie breaker may cut: cycles.toml key (resolution kind `zombie-safe`) and the static name reported to LeakCensus */
	struct ZombieSafeEdge {
		std::string_view edge;
		const char* name;
	};

	/**
	 * run(Player&), in this order. References to other world objects first (target, kisk, storage actors), then the observer lists: the
	 * stance and ride observers and the IdianStone listeners are also registered in the ObserveController, which is emptied last.
	 * L1 VisibleObject::breakTarget; L2 Player.setKisk(nullptr); L3 setOwner(nullptr) of the inventory, the regular warehouse and the account
	 * warehouse (Java does this at the end of leaveWorld); L4 PlayerController::breakStanceObserver; L5 clear Player.rideObservers;
	 * L6 IdianStone::breakActionListener of every equipped item; L7 ObserveController::clearWithoutNotify (observers and attack-calc observers).
	 */
	static constexpr std::array<Step, 8> LOGOUT_STEPS{{
		{"L1", "Player", "com.aionemu.gameserver.model.gameobjects.VisibleObject.target"},
		{"L2", "Player", "com.aionemu.gameserver.model.gameobjects.player.Player.kisk"},
		{"L3", "Player", "com.aionemu.gameserver.model.items.storage.PlayerStorage.actor"},
		{"L4", "Player", "com.aionemu.gameserver.controllers.PlayerController.stanceObserver"},
		{"L5", "Player", "com.aionemu.gameserver.model.gameobjects.player.Player.rideObservers"},
		{"L6", "Player", "com.aionemu.gameserver.model.items.IdianStone.actionListener"},
		{"L7", "Player", "com.aionemu.gameserver.controllers.ObserveController.observers"},
		{"L7", "Player", "com.aionemu.gameserver.controllers.ObserveController.attackCalcObservers"},
	}};

	/**
	 * onDelete(VisibleObject&), in this order, each step only for objects of its class. D3 empties the effect maps without ending the effects:
	 * a timed effect still ends through its end task (Effect.endEffect then removes its observers from other creatures), a permanent one no
	 * longer keeps the deleted creature. D4 removes the stat functions owned by effects, which would keep the Effect and so the creature.
	 */
	static constexpr std::array<Step, 8> DELETE_STEPS{{
		{"D1", "VisibleObject", "com.aionemu.gameserver.model.gameobjects.VisibleObject.target"},
		{"D2", "Creature", "com.aionemu.gameserver.controllers.ObserveController.observers"},
		{"D2", "Creature", "com.aionemu.gameserver.controllers.ObserveController.attackCalcObservers"},
		{"D3", "Creature", "com.aionemu.gameserver.controllers.effect.EffectController.abnormalEffectMap"},
		{"D3", "Creature", "com.aionemu.gameserver.controllers.effect.EffectController.passiveEffectMap"},
		{"D4", "Creature", "com.aionemu.gameserver.model.stats.container.CreatureGameStats.stats"},
		{"D5", "Npc", "com.aionemu.gameserver.model.gameobjects.Npc.walkerGroup"},
		{"D6", "Player", ""}, // run(player): LOGOUT_STEPS
	}};

	/**
	 * The zombie-safe edge list (cycles.toml resolutions of kind `zombie-safe`): the only references breakZombieEdges() cuts, each for objects
	 * of the class declaring it. The Player.summon and Player.pet links are cut on the Player side (Summon.master and Pet.master are final).
	 */
	static constexpr std::array<ZombieSafeEdge, 12> ZOMBIE_SAFE_EDGES{{
		{"com.aionemu.gameserver.model.gameobjects.VisibleObject.target", "target"},
		{"com.aionemu.gameserver.world.knownlist.KnownList.knownObjects", "knownObjects"},
		{"com.aionemu.gameserver.controllers.ObserveController.observers", "observers"},
		{"com.aionemu.gameserver.controllers.ObserveController.attackCalcObservers", "attackCalcObservers"},
		{"com.aionemu.gameserver.model.gameobjects.Npc.walkerGroup", "walkerGroup"},
		{"com.aionemu.gameserver.model.gameobjects.player.Player.kisk", "kisk"},
		{"com.aionemu.gameserver.model.items.storage.PlayerStorage.actor", "storageActor"},
		{"com.aionemu.gameserver.controllers.PlayerController.stanceObserver", "stanceObserver"},
		{"com.aionemu.gameserver.model.gameobjects.player.Player.rideObservers", "rideObservers"},
		{"com.aionemu.gameserver.model.items.IdianStone.actionListener", "idianStoneListener"},
		{"com.aionemu.gameserver.model.gameobjects.player.Player.summon", "summon"},
		{"com.aionemu.gameserver.model.gameobjects.player.Player.pet", "pet"},
	}};

	/** Logout breakers L1-L7 (see LOGOUT_STEPS). Idempotent; also valid for a Player that never entered the world. */
	static void run(Player& player) noexcept;

	/** Delete breakers D1-D6 of the object's kind (see DELETE_STEPS). Called last by VisibleObjectController::onDelete. */
	static void onDelete(VisibleObject& object) noexcept;

	/**
	 * Cuts the ZOMBIE_SAFE_EDGES of the object's kind that are still set and returns their names (static strings) for the LeakCensus warning.
	 * Runs on the instant pool in a normal TaskScope while the zombie is pinned (runtime::ZombieBreakable contract); exceptions propagate to
	 * LeakCensus, which logs them.
	 */
	static std::vector<const char*> breakZombieEdges(VisibleObject& object);

	LogoutBreakers() = delete;
};

} // namespace aion::gameserver::model::gameobjects::player
