#pragma once

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/runtime/sched/PinnedCallback.h"
#include "aion/gameserver/controllers/observer/ActionObserver.h"
#include "aion/gameserver/controllers/observer/fwd.h"
#include "aion/gameserver/model/gameobjects/fwd.h"

namespace aion::gameserver::controllers::observer {

/**
 * Runs an action when the observed creature dies.
 * <p>
 * S0c declaration header (docs/design/hub-headers.md §3.5). RefCounted ActionObserver (fieldmap K4, `Skill::firstTargetDieObserver`), created
 * with create(). The stored Consumer is a PinnedCallback (§7.3: capture rules of runtime-architecture.md §7.3).
 */
class DeathObserver : public ActionObserver {
	AION_MAKE_REF_FRIEND
private:
	const runtime::PinnedCallback<void(model::gameobjects::Creature&)> actionOnDeath;

protected:
	explicit DeathObserver(runtime::PinnedCallback<void(model::gameobjects::Creature&)> actionOnDeath);
	~DeathObserver() override;

public:
	/** Java: new DeathObserver(actionOnDeath) */
	static runtime::Ref<DeathObserver> create(runtime::PinnedCallback<void(model::gameobjects::Creature&)> actionOnDeath);

	void died(model::gameobjects::Creature& lastAttacker) override;
};

} // namespace aion::gameserver::controllers::observer
