#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/AggressiveNpcAI.h"

namespace aion::gameserver::handlers::ai::instance::raksang {

/**
 * The AI of the rubble in Raksang Ruins ("raksang_rubble"): it does not think; a second after its spawn it casts 19937 and goes 9 s
 * later; immune to abnormal states; dead it goes.
 * <p>
 * Java: data/handlers/ai/instance/raksang/RaksangRubbleAI.java, @AIName("raksang_rubble") (the marker is in the .cpp).
 *
 * @author xTz
 */
class RaksangRubbleAI : public AggressiveNpcAI {
public:
	explicit RaksangRubbleAI(Npc& owner) : AggressiveNpcAI(owner) {}

	bool canThink() override;

protected:
	void handleSpawned() override;

private:
	void startLifeTask();

public:
	bool ask(AIQuestion question) override;

protected:
	void handleDied() override;
};

} // namespace aion::gameserver::handlers::ai::instance::raksang
