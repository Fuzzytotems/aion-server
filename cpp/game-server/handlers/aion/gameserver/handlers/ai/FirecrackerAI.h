#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/GeneralNpcAI.h"

namespace aion::gameserver::handlers::ai {

/**
 * The AI of a firecracker ("firecracker"): a GeneralNpcAI under its own name.
 * <p>
 * Java: data/handlers/ai/FirecrackerAI.java, @AIName("firecracker").
 */
class FirecrackerAI : public GeneralNpcAI {
public:
	explicit FirecrackerAI(Npc& owner) : GeneralNpcAI(owner) {}
};

} // namespace aion::gameserver::handlers::ai
