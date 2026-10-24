#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"

namespace aion::gameserver::handlers::ai::instance::empyreanCrucible {

/**
 * Empyrean Crucible's administrator Arminos ("empadministratorarminos"): the announcements of the bonus stages 3 (217744) and 4 (217749).
 * <p>
 * Java: data/handlers/ai/instance/empyreanCrucible/EmpyreanAdministratorArminosAI.java, @AIName("empadministratorarminos") (the marker is in the .cpp).
 *
 * @author xTz
 */
class EmpyreanAdministratorArminosAI : public NpcAI {
public:
	explicit EmpyreanAdministratorArminosAI(Npc& owner) : NpcAI(owner) {}

protected:
	void handleSpawned() override;

private:
	void startEvent();
};

} // namespace aion::gameserver::handlers::ai::instance::empyreanCrucible
