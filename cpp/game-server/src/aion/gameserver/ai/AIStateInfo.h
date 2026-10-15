#pragma once

#include "aion/gameserver/ai/AIState.h"
#include "aion/gameserver/ai/event/AIEventType.h"

namespace aion::gameserver::ai {

/**
 * Companion of the generated enum AIState (docs/design/static-data.md §2.5, pattern ItemGroupInfo.h): Java's constructor data (the EnumSet of
 * AI events a state handles) and its method as a free function found by ADL (`canHandle(state, event)` for Java `state.canHandle(event)`).
 * <p>
 * Java: CREATED and DESPAWNED handle BEFORE_SPAWNED and SPAWNED, DIED handles DESPAWNED and DROP_REGISTERED, FORCED_WALKING handles
 * MOVE_ARRIVED, MOVE_VALIDATE, DESPAWNED and DIED; every other state handles all events (`EnumSet.allOf(AIEventType.class)`, NONE included).
 *
 * @author ATracer, Neon
 */

/** @return True, if the given event can be handled in this state. */
constexpr bool canHandle(AIState state, event::AIEventType event) noexcept {
	using enum event::AIEventType;
	switch (state) {
		case AIState::CREATED:
		case AIState::DESPAWNED:
			return event == BEFORE_SPAWNED || event == SPAWNED;
		case AIState::DIED:
			return event == DESPAWNED || event == DROP_REGISTERED;
		case AIState::FORCED_WALKING:
			return event == MOVE_ARRIVED || event == MOVE_VALIDATE || event == DESPAWNED || event == DIED;
		case AIState::IDLE:
		case AIState::WALKING:
		case AIState::FOLLOWING:
		case AIState::RETURNING:
		case AIState::FIGHT:
		case AIState::FEAR:
		case AIState::CONFUSE:
			return true;
	}
	return true; // not reached: every constant is listed
}

} // namespace aion::gameserver::ai
