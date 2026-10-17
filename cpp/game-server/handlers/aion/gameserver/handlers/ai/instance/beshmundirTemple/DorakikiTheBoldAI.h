#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/AggressiveNpcAI.h"

namespace aion::gameserver::handlers::ai::instance::beshmundirTemple {

/**
 * The AI of Dorakiki the Bold ("dorakiki_the_bold"): after each attack and back home she loses the effect 18901.
 * <p>
 * Java: data/handlers/ai/instance/beshmundirTemple/DorakikiTheBoldAI.java, @AIName("dorakiki_the_bold") (the marker is in the .cpp).
 *
 * @author unknown
 */
class DorakikiTheBoldAI : public AggressiveNpcAI {
public:
	explicit DorakikiTheBoldAI(Npc& owner) : AggressiveNpcAI(owner) {}

protected:
	void handleAttackComplete() override;

	void handleBackHome() override;
};

} // namespace aion::gameserver::handlers::ai::instance::beshmundirTemple
