#pragma once

#include "aion/gameserver/runtime/sched/PinnedCallback.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/utils/collections/fwd.h"

namespace aion::gameserver::utils::collections {

/**
 * C++: static-only classes (fieldmap K5). Java's `Predicate` constants are `static const runtime::PinnedCallback` defined in the .cpp from
 * captureless lambdas (hub-headers.md §7.3); the factories return callbacks that pin the player they capture, so a stored predicate keeps it
 * alive like Java's lambda does.
 *
 * @author ATracer, Neon
 */
class Predicates {
public:
	Predicates() = delete;

	/** Java: the raw ALWAYS_TRUE constant behind alwaysTrue(); one captureless lambda per element type */
	template <class T>
	static runtime::PinnedCallback<bool(T&)> alwaysTrue() {
		return runtime::PinnedCallback<bool(T&)>([](T&) { return true; });
	}

	class Players {
	public:
		Players() = delete;

		static const runtime::PinnedCallback<bool(model::gameobjects::player::Player&)> ONLINE;

		static const runtime::PinnedCallback<bool(model::gameobjects::player::Player&)> WITH_LOOT_PET;

		static runtime::PinnedCallback<bool(model::gameobjects::player::Player&)> sameRace(model::gameobjects::player::Player& p);

		static runtime::PinnedCallback<bool(model::gameobjects::player::Player&)> allExcept(model::gameobjects::player::Player& ignored);

		static runtime::PinnedCallback<bool(model::gameobjects::player::Player&)> canBeMentoredBy(model::gameobjects::player::Player& mentor);
	};
};

} // namespace aion::gameserver::utils::collections
