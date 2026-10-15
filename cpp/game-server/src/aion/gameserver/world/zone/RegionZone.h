#pragma once

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/geometry/RectangleArea.h"
#include "aion/gameserver/model/geometry/fwd.h"
#include "aion/gameserver/world/zone/fwd.h"

namespace aion::gameserver::world::zone {

/**
 * The rectangle of one map region (WORLD_REGION_SIZE square) between minZ and maxZ, used to find the zones a region intersects.
 * <p>
 * C++: RefCounted through RectangleArea (K3), created with create(...).
 *
 * @author ATracer
 */
class RegionZone : public model::geometry::RectangleArea {
	AION_MAKE_REF_FRIEND
protected:
	RegionZone(float startX, float startY, float minZ, float maxZ);
	~RegionZone() override;

public:
	/** Java: new RegionZone(startX, startY, minZ, maxZ) */
	static runtime::Ref<RegionZone> create(float startX, float startY, float minZ, float maxZ);

	bool isInside(model::geometry::AbstractArea& area);
};

} // namespace aion::gameserver::world::zone
