#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/GeneralNpcAI.h"

#include "aion/gameserver/runtime/fields/Atomic.h"

namespace aion::gameserver::handlers::ai::worlds {

/**
 * A nameless grave ("namelessgrave"): it does not think; its first death brings two 283905 around it.
 * <p>
 * Java: data/handlers/ai/worlds/NamelessGraveAI.java, @AIName("namelessgrave") (the marker is in the .cpp).
 *
 * @author Tibald
 */
class NamelessGraveAI : public GeneralNpcAI {
private:
	runtime::AtomicBoolean isSpawned{AION_LOCK_CLASS(NamelessGraveAI::isSpawned), false}; // Java: = new AtomicBoolean(false)

public:
	explicit NamelessGraveAI(Npc& owner) : GeneralNpcAI(owner) {}

	bool canThink() override;

protected:
	void handleDied() override;
};

} // namespace aion::gameserver::handlers::ai::worlds
