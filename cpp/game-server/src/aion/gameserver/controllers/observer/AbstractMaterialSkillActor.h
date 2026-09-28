#pragma once

#include <cstdint>
#include <vector>

#include "aion/gameserver/runtime/collections/Rc.h"
#include "aion/gameserver/runtime/fields/Atomic.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/runtime/sched/Future.h"
#include "aion/gameserver/controllers/observer/AbstractCollisionObserver.h"
#include "aion/gameserver/controllers/observer/fwd.h"
#include "aion/gameserver/geoEngine/scene/fwd.h"
#include "aion/gameserver/model/fwd.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/templates/materials/fwd.h"

namespace aion::gameserver::controllers::observer {

/**
 * Applies material skills (e.g. fire, water) to a creature that touches a material geometry, once per second while touched.
 * <p>
 * S0c declaration header (docs/design/hub-headers.md §3.5). RefCounted ActionObserver (fieldmap K4). The skill task (Java inner class
 * MaterialSkillTask, a Runnable scheduled at a fixed rate) is used only by bodies: declared here, defined in the .cpp as a K4 RefCounted class
 * holding the actor (§9.3); abort() is the java-hook that cancels it. The skills list parameter is stored (§7.1: by value).
 *
 * @author Yeats, Neon
 */
class AbstractMaterialSkillActor : public AbstractCollisionObserver {
	AION_MAKE_REF_FRIEND
private:
	/** Java: private class MaterialSkillTask implements Runnable (defined in the .cpp with the port of act()) */
	class MaterialSkillTask;

	runtime::AtomicReference<runtime::FutureRef> task{AION_LOCK_CLASS(AbstractMaterialSkillActor::task)};
	const model::TaskId taskId;

protected:
	runtime::Field<runtime::Ref<runtime::RcArrayList<const model::templates::materials::MaterialSkill*>>> skills{};
	runtime::Field<bool> isTouched{false};

	AbstractMaterialSkillActor(model::gameobjects::Creature& creature, runtime::Ptr<geoEngine::scene::Spatial> geometry, int8_t intentions,
		CheckType checkType, model::TaskId taskId, std::vector<const model::templates::materials::MaterialSkill*> skills);
	~AbstractMaterialSkillActor() override;

public:
	void act();

	void abort();

	void died(model::gameobjects::Creature& creature) override;

private:
	// synchronized (skills)
	const model::templates::materials::MaterialSkill* findFirstSkillWithMatchingCondition();

	bool matchActConditions(const model::templates::materials::MaterialSkill* skill);
};

} // namespace aion::gameserver::controllers::observer
