#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/geometry/SphereArea.h"
#include "aion/gameserver/model/geometry/fwd.h"
#include "aion/gameserver/world/zone/fwd.h"

namespace aion::gameserver::model::geometry {

/**
 * C++: fieldmap K4 (RefCounted through SphereArea); created with create(...).
 *
 * @author Rolandas
 */
class SemisphereArea : public SphereArea {
	AION_MAKE_REF_FRIEND
protected:
	SemisphereArea(const world::zone::ZoneName* zoneName, int32_t worldId, float x, float y, float z, float r);

public:
	static runtime::Ref<SemisphereArea> create(const world::zone::ZoneName* zoneName, int32_t worldId, float x, float y, float z, float r);

	bool isInside3D(Point3D& point) override;

	bool isInside3D(float x, float y, float z) override;

	bool isInsideZ(Point3D& point) override;
	using SphereArea::isInsideZ;

	float getMinZ() override;

	float getMaxZ() override;

	double getDistance3D(Point3D& point) override;

	double getDistance3D(float x, float y, float z) override;

	bool intersectsRectangle(RectangleArea& area) override;

protected:
	~SemisphereArea() override;
};

} // namespace aion::gameserver::model::geometry
