#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/controllers/attack/AttackStatus.h"
#include "aion/gameserver/controllers/observer/AttackCalcObserver.h"
#include "aion/gameserver/controllers/observer/fwd.h"

namespace aion::gameserver::controllers::observer {

/**
 * Attack calc observer with a countdown of critical hits for the attacker (BoostHateEffect-like anonymous subclasses in skillengine.effect).
 * <p>
 * RefCounted (fieldmap K4, class tree of AttackCalcObserver). Java's anonymous subclasses are callback structs in the creating effect's .cpp
 * (hub-headers.md §7.3).
 *
 * @author kecimis
 */
class AttackerCriticalStatusObserver : public AttackCalcObserver {
	AION_MAKE_REF_FRIEND
protected:
	runtime::Field<runtime::Ref<AttackerCriticalStatus>> acStatus{};
	runtime::Field<attack::AttackStatus> status;

	AttackerCriticalStatusObserver(attack::AttackStatus status, int32_t count, int32_t value, bool isPercent);
	~AttackerCriticalStatusObserver() override;

public:
	/** Java: new AttackerCriticalStatusObserver(status, count, value, isPercent) */
	static runtime::Ref<AttackerCriticalStatusObserver> create(attack::AttackStatus status, int32_t count, int32_t value, bool isPercent);

	int32_t getCount();

	void decreaseCount();
};

} // namespace aion::gameserver::controllers::observer
