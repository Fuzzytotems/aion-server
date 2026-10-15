#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/NoActionAI.h"

namespace aion::gameserver::handlers::ai::instance::rentusBase {

/**
 * The AI of the Drakan barricade in Rentus Base ("drakanbarricade"): no loot.
 * <p>
 * Java: data/handlers/ai/instance/rentusBase/DrakanBarricadeAI.java, @AIName("drakanbarricade") (the marker is in the .cpp).
 *
 * @author Sykra
 */
class DrakanBarricadeAI : public NoActionAI {
public:
	explicit DrakanBarricadeAI(Npc& owner) : NoActionAI(owner) {}

	bool ask(AIQuestion question) override;
};

} // namespace aion::gameserver::handlers::ai::instance::rentusBase
