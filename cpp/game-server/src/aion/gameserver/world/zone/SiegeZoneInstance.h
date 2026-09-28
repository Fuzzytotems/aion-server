#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/model/templates/zone/fwd.h"
#include "aion/gameserver/world/zone/ZoneInstance.h"
#include "aion/gameserver/world/zone/fwd.h"

namespace aion::gameserver::world::zone {

/**
 * S0c declaration header (docs/design/hub-headers.md §3.5), written for the member-type guards waiting for it at the spine freeze.
 *
 * @author MrPoke
 */
class SiegeZoneInstance : public ZoneInstance {
	AION_MAKE_REF_FRIEND
protected:
	SiegeZoneInstance(int32_t mapId, model::templates::zone::ZoneInfo& template_);

public:
	static runtime::Ref<SiegeZoneInstance> create(int32_t value, model::templates::zone::ZoneInfo& template_Value);

protected:
	~SiegeZoneInstance() override;
};

} // namespace aion::gameserver::world::zone
