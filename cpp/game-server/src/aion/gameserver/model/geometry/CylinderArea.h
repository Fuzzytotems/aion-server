#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/geometry/AbstractArea.h"
#include "aion/gameserver/model/geometry/fwd.h"
#include "aion/gameserver/model/templates/zone/fwd.h"
#include "aion/gameserver/world/zone/fwd.h"

namespace aion::gameserver::model::geometry {

/**
 * This class implements cylinder area
 * <p>
 * C++: fieldmap K3 (immutable), RefCounted through AbstractArea; created with create(...). getClosestPoint(float, float) is ported as
 * getClosestPoint2D (AbstractArea class comment).
 *
 * @author SoulKeeper
 */
class CylinderArea : public AbstractArea {
	AION_MAKE_REF_FRIEND
private:
	/** Center of cylinder */
	const float centerX;
	/** Center of cylinder */
	const float centerY;
	/** Cylinder radius */
	const float radius;

protected:
	/** Creates new cylinder with given radius */
	CylinderArea(const world::zone::ZoneName* zoneName, int32_t worldId, const templates::zone::Point2D* center, float radius, float minZ,
		float maxZ);

public:
	static runtime::Ref<CylinderArea> create(const world::zone::ZoneName* zoneName, int32_t worldId, const templates::zone::Point2D* center,
		float radius, float minZ, float maxZ);

protected:
	/** Creates new cylider with given radius */
	CylinderArea(const world::zone::ZoneName* zoneName, int32_t worldId, float x, float y, float radius, float minZ, float maxZ);

public:
	static runtime::Ref<CylinderArea> create(const world::zone::ZoneName* zoneName, int32_t worldId, float x, float y, float radius, float minZ,
		float maxZ);

	bool isInside2D(float x, float y) override;
	using AbstractArea::isInside2D;

	double getDistance2D(float x, float y) override;
	using AbstractArea::getDistance2D;

	double getDistance3D(float x, float y, float z) override;
	using AbstractArea::getDistance3D;

	/** The Java body is getClosestPoint2D (AbstractArea class comment) */
	std::optional<templates::zone::Point2D> getClosestPoint(float x, float y) override;
	using AbstractArea::getClosestPoint;

	templates::zone::Point2D getClosestPoint2D(float x, float y) override;

	bool intersectsRectangle(RectangleArea& area) override;

protected:
	~CylinderArea() override;
};

} // namespace aion::gameserver::model::geometry
