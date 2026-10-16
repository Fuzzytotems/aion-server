#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/GeneralNpcAI.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include <string>

namespace aion::gameserver::handlers::ai::instance::empyreanCrucible {

/**
 * Empyrean Crucible's draky ("arminos_draky"): walks its route twice, never thinks, and is gone at the second lap's end.
 * <p>
 * Java: data/handlers/ai/instance/empyreanCrucible/ArminosDrakyAI.java, @AIName("arminos_draky") (the marker is in the .cpp).
 *
 * @author xTz
 */
class ArminosDrakyAI : public GeneralNpcAI {
private:
	const std::string walkerId = "300300001"; // Java: = "300300001"
	runtime::Field<bool> isStart{true};

public:
	explicit ArminosDrakyAI(Npc& owner) : GeneralNpcAI(owner) {}

protected:
	void handleSpawned() override;

	void handleMoveArrived() override;

public:
	bool ask(AIQuestion question) override;

	bool canThink() override;
};

} // namespace aion::gameserver::handlers::ai::instance::empyreanCrucible
