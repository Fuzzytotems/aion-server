#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"

namespace aion::gameserver::handlers::ai::siege {

/**
 * A fortress gate ("fortressgate"): talking to it asks STR_ASK_PASS_BY_GATE, and an accepted question within talk range moves the player through
 * it (in front of it from behind, behind it from the front); its death deletes its door repair stone; AP and the shared rewards, no loot, no
 * respawn.
 * <p>
 * Java: data/handlers/ai/siege/FortressGateAI.java, @AIName("fortressgate") (the marker is in the .cpp).
 *
 * @author Source
 */
class FortressGateAI : public NpcAI {
public:
	explicit FortressGateAI(Npc& owner) : NpcAI(owner) {}

protected:
	void handleDialogStart(Player& player) override;

public:
	bool ask(AIQuestion question) override;

protected:
	void handleDied() override;
};

} // namespace aion::gameserver::handlers::ai::siege
