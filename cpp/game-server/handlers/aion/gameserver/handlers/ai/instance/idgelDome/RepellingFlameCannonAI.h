#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"

#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/sched/Future.h"

namespace aion::gameserver::handlers::ai::instance::idgelDome {

/**
 * The AI of the repelling flame cannon in Idgel Dome ("repelling_flame_cannon"): no damage; skill 21648 every second.
 * <p>
 * Java: data/handlers/ai/instance/idgelDome/RepellingFlameCannonAI.java, @AIName("repelling_flame_cannon") (the marker is in the .cpp).
 *
 * @author Ritsu, Estrayl
 */
class RepellingFlameCannonAI : public NpcAI {
private:
	runtime::Field<runtime::FutureRef> skillTask{};

public:
	explicit RepellingFlameCannonAI(Npc& owner) : NpcAI(owner) {}

	float modifyDamage(Creature& attacker, float damage, runtime::Ptr<Effect> effect) override;

protected:
	void handleSpawned() override;

	void handleDespawned() override;
};

} // namespace aion::gameserver::handlers::ai::instance::idgelDome
