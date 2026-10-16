#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/siege/SiegeNpcAI.h"

#include "aion/gameserver/runtime/fields/Field.h"

namespace aion::gameserver::handlers::ai::siege {

/**
 * The commander a Balaur dredgion drops into a fortress ("dredgion_commander"): 3 s after its spawn it hates the fortress boss it knows (a
 * GCHIEF_LIGHT or GCHIEF_DARK npc), 25 m from its spawn it goes for him; it hits him and has max hp by the fortress protector multiplier;
 * its death tells the fortress assault.
 * <p>
 * Java: data/handlers/ai/siege/DredgionCommanderAI.java, @AIName("dredgion_commander") (the marker is in the .cpp). C++: the search task is
 * pinned to this AI.
 *
 * @author Luzien, Estrayl
 */
class DredgionCommanderAI : public SiegeNpcAI {
private:
	runtime::Field<runtime::Ref<Npc>> fortressBoss{}; // Java: private Npc fortressBoss

public:
	explicit DredgionCommanderAI(Npc& owner) : SiegeNpcAI(owner) {}

protected:
	void handleSpawned() override;

private:
	void findFortressBoss();

protected:
	void handleMoveArrived() override;

public:
	float modifyOwnerDamage(float damage, Creature& effected, runtime::Ptr<Effect> effect) override;

	void modifyOwnerStat(Stat2& stat) override;

protected:
	void handleDespawned() override;

	void handleDied() override;
};

} // namespace aion::gameserver::handlers::ai::siege
