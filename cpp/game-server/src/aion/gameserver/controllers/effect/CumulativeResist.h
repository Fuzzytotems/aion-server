#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/controllers/effect/fwd.h"

namespace aion::gameserver::controllers::effect {

/**
 * The cumulative resistance level of a player against one crowd control type.
 * <p>
 * S0c declaration header (docs/design/hub-headers.md §3.5): the element type of `PlayerEffectController::cumulativeResistInfo`. RefCounted
 * (fieldmap K4), created with create(); Java package-private (all members public in C++).
 */
class CumulativeResist : public runtime::RefCounted {
	AION_MAKE_REF_FRIEND
private:
	runtime::Field<int32_t> level{};
	runtime::Field<int64_t> expirationTime{};

protected:
	CumulativeResist();
	~CumulativeResist() override;

public:
	/** Java: new CumulativeResist() */
	static runtime::Ref<CumulativeResist> create();

	void tryIncrementLevel(int64_t maxDurationMillis);

	float getDurationMultiplier();

	int32_t getCooldownTimeOffset(CumulativeResistType type);

	int32_t getResistance();

private:
	void resetIfExpired();
};

} // namespace aion::gameserver::controllers::effect
