#pragma once

#include <vector>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/controllers/attack/fwd.h"
#include "aion/gameserver/controllers/observer/fwd.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/skillengine/model/fwd.h"

namespace aion::gameserver::controllers::observer {

/**
 * Observer consulted during attack calculation (shields, blocks, damage multipliers); the base answers "no effect".
 * <p>
 * S0c declaration header (docs/design/hub-headers.md §3.5). RefCounted (fieldmap K3; Java has no base class), held by
 * `ObserveController::attackCalcObservers`. Java's anonymous subclasses are callback structs in the creating class's .cpp with their own
 * create (§7.3). The literal returns and the empty checkShield body are ported inline.
 *
 * @author ATracer
 */
class AttackCalcObserver : public runtime::RefCounted {
	AION_MAKE_REF_FRIEND
protected:
	AttackCalcObserver();
	~AttackCalcObserver() override;

public:
	/** Java: new AttackCalcObserver() */
	static runtime::Ref<AttackCalcObserver> create();

	/**
	 * @return false
	 */
	virtual bool checkStatus(attack::AttackStatus status) { return false; }

	/**
	 * @param effect null for auto attacks (AttackUtil.java:51 passes null through ObserveController.checkShieldStatus)
	 */
	virtual void checkShield(const std::vector<runtime::Ptr<attack::AttackResult>>& attackList, runtime::Ptr<skillengine::model::Effect> effect,
		model::gameobjects::Creature& attacker) {}

	virtual bool checkAttackerStatus(attack::AttackStatus status) { return false; }

	/** @return a new AttackerCriticalStatus(false) in the base class */
	virtual runtime::Ref<AttackerCriticalStatus> checkAttackerCriticalStatus(attack::AttackStatus status, bool isSkill);

	/**
	 * @return physical damage multiplier
	 */
	virtual float getBasePhysicalDamageMultiplier(bool isSkill) { return 1.0f; }

	/**
	 * @return magic damage multiplier
	 */
	virtual float getBaseMagicalDamageMultiplier() { return 1.0f; }
};

} // namespace aion::gameserver::controllers::observer
