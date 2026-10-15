#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/AggressiveNpcAI.h"
#include "aion/gameserver/runtime/fields/Atomic.h"

namespace aion::gameserver::handlers::ai::instance::raksang {

/**
 * The AI of the seal generators in Raksang Ruins ("seal_generator"): it does not think; the first player within 30 m makes it announce itself
 * to the map; immune to abnormal states; any hit does 1 damage.
 * <p>
 * Java: data/handlers/ai/instance/raksang/SealGeneratorAI.java, @AIName("seal_generator") (the marker is in the .cpp).
 *
 * @author xTz
 */
class SealGeneratorAI : public AggressiveNpcAI {
private:
	runtime::AtomicBoolean startedEvent{AION_LOCK_CLASS(SealGeneratorAI::startedEvent)}; // Java: = new AtomicBoolean()

public:
	explicit SealGeneratorAI(Npc& owner) : AggressiveNpcAI(owner) {}

	bool canThink() override;

protected:
	void handleCreatureMoved(Creature& creature) override;

public:
	bool ask(AIQuestion question) override;

	float modifyDamage(Creature& attacker, float damage, runtime::Ptr<Effect> effect) override;
};

} // namespace aion::gameserver::handlers::ai::instance::raksang
