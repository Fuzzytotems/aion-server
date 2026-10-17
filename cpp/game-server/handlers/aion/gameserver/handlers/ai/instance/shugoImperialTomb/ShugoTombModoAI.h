#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/instance/shugoImperialTomb/ShugoTombAttackerAI.h"

namespace aion::gameserver::handlers::ai::instance::shugoImperialTomb {

/**
 * The AI of the Shugo Imperial Tomb's Modo ("shugo_tomb_modo"): an attacker that hates the towers within 30 m as soon as it spawns.
 * <p>
 * Java: data/handlers/ai/instance/shugoImperialTomb/ShugoTombModoAI.java, @AIName("shugo_tomb_modo") (the marker is in the .cpp).
 *
 * @author Ritsu
 */
class ShugoTombModoAI : public ShugoTombAttackerAI {
public:
	explicit ShugoTombModoAI(Npc& owner) : ShugoTombAttackerAI(owner) {}

protected:
	void handleSpawned() override;
};

} // namespace aion::gameserver::handlers::ai::instance::shugoImperialTomb
