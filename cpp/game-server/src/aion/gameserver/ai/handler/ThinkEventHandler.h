#pragma once

#include "aion/gameserver/ai/fwd.h"
#include "aion/gameserver/ai/handler/fwd.h"

namespace aion::gameserver::ai::handler {

/**
 * The think() of an NPC AI: one thought at a time, and what it does in each AI state.
 * <p>
 * A static-only utility class (hub-headers.md §11.1). `setThinking()` returning false is what **drops** a think; a reentrant
 * implementation would turn one dropped think into an endless attack loop (m5b-plan.md §8 risk 3).
 *
 * @author ATracer
 */
class ThinkEventHandler {
public:
	ThinkEventHandler() = delete;

	static void onThink(NpcAI& npcAI);

private:
	static void thinkInInactiveRegion(NpcAI& npcAI);

public:
	static void thinkAttack(NpcAI& npcAI);

	static void thinkIdle(NpcAI& npcAI);

private:
	static bool shouldResetHeading(NpcAI& npcAI);
};

} // namespace aion::gameserver::ai::handler
