#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/controllers/attack/AttackStatus.h"
#include "aion/gameserver/controllers/observer/AttackCalcObserver.h"
#include "aion/gameserver/controllers/observer/fwd.h"

namespace aion::gameserver::controllers::observer {

/**
 * Attack calc observer carrying a value and an attack status for the Always*Effect, BlindEffect and similar anonymous subclasses.
 * <p>
 * RefCounted (fieldmap K4, class tree of AttackCalcObserver). Java's anonymous subclasses are callback structs in the creating effect's .cpp
 * (hub-headers.md §7.3); they call the protected constructor.
 *
 * @author ATracer
 */
class AttackStatusObserver : public AttackCalcObserver {
	AION_MAKE_REF_FRIEND
protected:
	runtime::Field<int32_t> value;
	runtime::Field<attack::AttackStatus> status;

	AttackStatusObserver(int32_t value, attack::AttackStatus status);
	~AttackStatusObserver() override;

public:
	/** Java: new AttackStatusObserver(value, status) */
	static runtime::Ref<AttackStatusObserver> create(int32_t value, attack::AttackStatus status);
};

} // namespace aion::gameserver::controllers::observer
