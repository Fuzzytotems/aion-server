#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/portals/PortalAI.h"
#include "aion/gameserver/runtime/fields/Field.h"

namespace aion::gameserver::handlers::ai::instance::kamarBf {

/**
 * The AI of the Kamar Battlefield teleporters ("kamar_teleport"): a portal of 25 uses, the remaining uses told after each.
 * <p>
 * Java: data/handlers/ai/instance/kamarBf/KamarTeleportAI.java, @AIName("kamar_teleport") (the marker is in the .cpp).
 *
 * @author Luzien
 */
class KamarTeleportAI : public portals::PortalAI {
protected:
	runtime::Field<int32_t> remainingUses{};

public:
	explicit KamarTeleportAI(Npc& owner) : portals::PortalAI(owner) {}

protected:
	void handleSpawned() override;

	void handleUseItemFinish(Player& player) override;
};

} // namespace aion::gameserver::handlers::ai::instance::kamarBf
