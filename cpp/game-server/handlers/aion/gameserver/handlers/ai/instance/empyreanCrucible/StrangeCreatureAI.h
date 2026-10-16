#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/GeneralNpcAI.h"

namespace aion::gameserver::handlers::ai::instance::empyreanCrucible {

/**
 * The strange creatures of Empyrean Crucible ("strange_creature"): a shout and a skill after half a second, gone after 6.5 s.
 * <p>
 * Java: data/handlers/ai/instance/empyreanCrucible/StrangeCreatureAI.java, @AIName("strange_creature") (the marker is in the .cpp).
 *
 * @author Luzien
 */
class StrangeCreatureAI : public GeneralNpcAI {
public:
	explicit StrangeCreatureAI(Npc& owner) : GeneralNpcAI(owner) {}

protected:
	void handleSpawned() override;

private:
	void startEventTask();

	void startLifeTask();
};

} // namespace aion::gameserver::handlers::ai::instance::empyreanCrucible
