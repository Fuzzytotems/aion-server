#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/GeneralNpcAI.h"

namespace aion::gameserver::handlers::ai::instance::empyreanCrucible {

/**
 * RM-1337's sparks ("spark_of_darkness"): a skill after half a second, gone after 6.5 s.
 * <p>
 * Java: data/handlers/ai/instance/empyreanCrucible/SparkOfDarknessAI.java, @AIName("spark_of_darkness") (the marker is in the .cpp).
 *
 * @author Luzien
 */
class SparkOfDarknessAI : public GeneralNpcAI {
public:
	explicit SparkOfDarknessAI(Npc& owner) : GeneralNpcAI(owner) {}

protected:
	void handleSpawned() override;

private:
	void startEventTask();

	void startLifeTask();
};

} // namespace aion::gameserver::handlers::ai::instance::empyreanCrucible
