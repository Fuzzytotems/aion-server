#pragma once

#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/controllers/fwd.h"
#include "aion/gameserver/model/gameobjects/fwd.h"

namespace aion::gameserver::controllers {

/**
 * The controller of traps: unregisters the trap from TrapService when it dies or is deleted.
 */
class TrapController : public NpcController {
public:
	/** Java: implicit default constructor */
	TrapController();
	~TrapController() override;

	void onDie(model::gameobjects::Creature& lastAttacker) override;

	void onDelete() override;
};

} // namespace aion::gameserver::controllers
