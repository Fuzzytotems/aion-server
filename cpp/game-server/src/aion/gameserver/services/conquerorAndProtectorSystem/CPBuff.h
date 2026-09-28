#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/stats/calc/StatOwner.h"
#include "aion/gameserver/model/templates/cp/fwd.h"
#include "aion/gameserver/services/conquerorAndProtectorSystem/fwd.h"

namespace aion::gameserver::services::conquerorAndProtectorSystem {

/**
 * S0c declaration header (docs/design/hub-headers.md §3.5), written for the member-type guards waiting for it at the spine freeze.
 *
 * @author Dtem
 */
class CPBuff : public runtime::RefCounted, public model::stats::calc::StatOwner {
	AION_MAKE_REF_FRIEND
protected:
	/** Java: the implicit default constructor */
	CPBuff();

public:
	static runtime::Ref<CPBuff> create();

	void applyEffect(model::gameobjects::player::Player& player, model::templates::cp::CPType type, int32_t rank);

	void endEffect(model::gameobjects::player::Player& player);

	/** C++ only: StatOwner retain the object itself. */
	void retain() const noexcept override { runtime::RefCounted::retain(); }

	void release() const noexcept override { runtime::RefCounted::release(); }

protected:
	~CPBuff() override;
};

} // namespace aion::gameserver::services::conquerorAndProtectorSystem
