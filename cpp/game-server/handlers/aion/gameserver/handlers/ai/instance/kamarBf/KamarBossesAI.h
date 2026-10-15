#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/AggressiveNpcAI.h"
#include "aion/gameserver/runtime/fields/Field.h"

namespace aion::gameserver::handlers::ai::instance::kamarBf {

/**
 * The AI of the Kamar Battlefield generals ("kamarbosses"): a flag npc at the spawn, deleted with the boss; messages to the map when
 * attacked and at the death.
 * <p>
 * Java: data/handlers/ai/instance/kamarBf/KamarBossesAI.java, @AIName("kamarbosses") (the marker is in the .cpp).
 *
 * @author Cheatkiller
 */
class KamarBossesAI : public AggressiveNpcAI {
private:
	runtime::Field<runtime::Ref<Npc>> flag{}; // Java: Npc flag (null until spawned)

public:
	explicit KamarBossesAI(Npc& owner) : AggressiveNpcAI(owner) {}

protected:
	void handleAttack(runtime::Ptr<Creature> creature) override;

	void handleSpawned() override;

	void handleDespawned() override;

	void handleDied() override;
};

} // namespace aion::gameserver::handlers::ai::instance::kamarBf
