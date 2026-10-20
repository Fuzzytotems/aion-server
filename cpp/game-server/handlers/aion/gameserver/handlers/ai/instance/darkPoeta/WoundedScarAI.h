#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/GeneralNpcAI.h"
#include "aion/gameserver/runtime/fields/Atomic.h"

namespace aion::gameserver::handlers::ai::instance::darkPoeta {

/**
 * Dark Poeta's wounded Scar ("wounded_scar"): talked to, walks his route invulnerable, calling adds at steps 8, 15, 25 and 26, and at its
 * end becomes 281116.
 * <p>
 * Java: data/handlers/ai/instance/darkPoeta/WoundedScarAI.java, @AIName("wounded_scar") (the marker is in the .cpp).
 *
 * @author Estrayl
 */
class WoundedScarAI : public GeneralNpcAI {
private:
	runtime::AtomicBoolean isDestinationReached{AION_LOCK_CLASS(WoundedScarAI::isDestinationReached)}; // Java: = new AtomicBoolean()

public:
	explicit WoundedScarAI(Npc& owner) : GeneralNpcAI(owner) {}

	bool canThink() override;

protected:
	void handleDialogStart(Player& player) override;

	void handleMoveArrived() override;
};

} // namespace aion::gameserver::handlers::ai::instance::darkPoeta
