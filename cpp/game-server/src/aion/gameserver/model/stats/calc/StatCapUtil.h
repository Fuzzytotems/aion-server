#pragma once

#include <cstdint>
#include <functional>
#include <map>

#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/stats/calc/fwd.h"
#include "aion/gameserver/model/stats/container/fwd.h"

namespace aion::gameserver::model::stats::calc {

/**
 * The lower and upper caps of stats (and the PvP difference limits), applied at the end of every stat calculation.
 * <p>
 * C++: a static-only class. Java's `EnumMap limits` is filled only by the static initializer (registerDefaults), so it is a `static const`
 * std::map built by registerDefaults() (hub-headers.md §11.1: no Monitor is taken during static initialization). The private functional
 * interface CapFunction is a `std::function` (its lambdas capture only constants; they read the creature argument), the private records Cap
 * and StatCapRule are plain value structs.
 *
 * @author ATracer, Neon
 */
class StatCapUtil {
public:
	StatCapUtil() = delete;

private:
	/** Java: private record Cap(int min, int max) */
	struct Cap {
		int32_t min;
		int32_t max;
	};

	/** Java: private functional interface CapFunction { int apply(Creature creature); } */
	using CapFunction = std::function<int32_t(gameobjects::Creature& creature)>;

	/** Java: private record StatCapRule(CapFunction lowerCap, CapFunction upperCap, int diffLimit) */
	struct StatCapRule {
		// fieldmap: a value record of the immutable rule table (static data built once by the static initializer), no shared object
		const CapFunction lowerCap;
		// fieldmap: a value record of the immutable rule table (static data built once by the static initializer), no shared object
		const CapFunction upperCap;
		// fieldmap: a value record of the immutable rule table (static data built once by the static initializer), no shared object
		const int32_t diffLimit;

		// fieldmap: the named constant is an immutable value of the rule table, not a RefCounted object
		static const StatCapRule UNLIMITED;
	};

	static const std::map<container::StatEnum, StatCapRule> limits;

	/** Java: the static initializer's registerDefaults() filling `limits`; C++ returns the map the static member is initialized with */
	static std::map<container::StatEnum, StatCapRule> registerDefaults();

public:
	static int32_t getElementalDefenseBaseValue();

	static void calculateBaseValue(Stat2& stat, gameobjects::Creature& creature);

	static int32_t getLowerCap(container::StatEnum stat, gameobjects::Creature& creature);

	static int32_t getUpperCap(container::StatEnum stat, gameobjects::Creature& creature);

	static int32_t getElementalDefenseCapForCreature(gameobjects::Creature& creature);

	static int32_t getDifferenceLimit(container::StatEnum stat);

	/** @throws IllegalArgumentException if the lower cap is greater than the upper cap (Java Math.clamp) */
	static int32_t clampStatValue(container::StatEnum stat, gameobjects::Creature& creature, int32_t value);

	static int32_t limitValueForPvpOrPveStat(container::CombatMode mode, container::RatioType type, int32_t value);

private:
	static void cap(Stat2& stat2, int32_t lowerCap, int32_t upperCap);

	// Java register(...) overloads: C++ builds into the map under construction (the keyword rule renames register to register_)
	static void register_(std::map<container::StatEnum, StatCapRule>& limits, container::StatEnum stat, int32_t lowerCap, int32_t upperCap);

	static void register_(std::map<container::StatEnum, StatCapRule>& limits, container::StatEnum stat, CapFunction lowerCap, CapFunction upperCap);

	static void register_(std::map<container::StatEnum, StatCapRule>& limits, container::StatEnum stat, int32_t lowerCap, CapFunction upperCap);

	static void register_(std::map<container::StatEnum, StatCapRule>& limits, container::StatEnum stat, int32_t lowerCap, CapFunction upperCap,
		int32_t diffLimit);

	/** @throws IllegalArgumentException if a limit for the stat is already registered */
	static void register_(std::map<container::StatEnum, StatCapRule>& limits, container::StatEnum stat, CapFunction lowerCap, CapFunction upperCap,
		int32_t diffLimit);

	static const StatCapRule& getRule(container::StatEnum stat);
};

} // namespace aion::gameserver::model::stats::calc
