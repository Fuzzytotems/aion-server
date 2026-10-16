#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/controllers/observer/fwd.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/skillengine/effect/fwd.h"
#include "aion/gameserver/skillengine/model/fwd.h"

namespace aion::gameserver::controllers::observer {

/**
 * Hub header (docs/design/hub-headers.md). RefCounted: observers are held by ObserveController and by their creators as `Ref<ActionObserver>`.
 * Java's anonymous subclasses (`new ActionObserver(ObserverType.X) { ... }`) are the callback structs of fieldmap.py (`X_ActionObserver`),
 * defined in the creating class's .cpp, deriving from this class and constructed through their own `create` (§7.3).
 *
 * @author ATracer
 */
class ActionObserver : public runtime::RefCounted {
	AION_MAKE_REF_FRIEND
private:
	runtime::Field<bool> oneTimeUse{false};
	const ObserverType observerType;

protected:
	explicit ActionObserver(ObserverType observerType);
	~ActionObserver() override;

public:
	/** Java: new ActionObserver(observerType) */
	static runtime::Ref<ActionObserver> create(ObserverType observerType);

	void makeOneTimeUse() { oneTimeUse.set(true); }

	bool isOneTimeUse() const { return oneTimeUse.get(); }

	/**
	 * Called when the observer was removed and no longer receives events
	 */
	virtual void onRemoved() {}

	ObserverType getObserverType() const { return observerType; }

	virtual void moved() {}

	/**
	 * @param creature who effected
	 * @param skillId - effector skill id, which called this method
	 */
	virtual void attacked(model::gameobjects::Creature& creature, int32_t skillId) {}

	virtual void attack(model::gameobjects::Creature& creature, int32_t skillId) {}

	virtual void equip(model::gameobjects::Item& item, model::gameobjects::player::Player& owner) {}

	virtual void unequip(model::gameobjects::Item& item, model::gameobjects::player::Player& owner) {}

	virtual void startSkillCast(skillengine::model::Skill& skill) {}

	virtual void endSkillCast(skillengine::model::Skill& skill) {}

	virtual void boostSkillCost(skillengine::model::Skill& skill) {}

	virtual void died(model::gameobjects::Creature& lastAttacker) {}

	virtual void dotattacked(model::gameobjects::Creature& creature, skillengine::model::Effect& dotEffect) {}

	virtual void itemused(model::gameobjects::Item& item) {}

	virtual void abnormalsetted(skillengine::effect::AbnormalState state) {}

	virtual void summonrelease() {}

	virtual void sit() {}

	virtual void hpChanged(int32_t value) {}
};

} // namespace aion::gameserver::controllers::observer
