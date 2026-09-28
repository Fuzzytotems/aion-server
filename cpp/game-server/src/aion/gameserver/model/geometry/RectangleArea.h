#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/geometry/AbstractArea.h"
#include "aion/gameserver/model/geometry/fwd.h"
#include "aion/gameserver/model/templates/zone/fwd.h"
#include "aion/gameserver/world/zone/fwd.h"

namespace aion::gameserver::model::geometry {

/**
 * Rectangle area, most wide spread in the game
 * <p>
 * C++: fieldmap K3 (immutable), RefCounted through AbstractArea; created with create(...).
 * getClosestPoint(float, float) is ported as getClosestPoint2D (AbstractArea class comment). Deviation: the Java constructor taking four
 * java.awt.Point corners is not ported: it has no caller and the port has no java.awt types (DEVIATIONS).
 *
 * @author SoulKeeper
 */
class RectangleArea : public AbstractArea {
	AION_MAKE_REF_FRIEND
private:
	/** Min x point */
	const float minX;

public:
	float getMinX() const { return minX; }

	float getMaxX() const { return maxX; }

	float getMinY() const { return minY; }

	float getMaxY() const { return maxY; }

private:
	/** Max x point */
	const float maxX;
	/** Min y point */
	const float minY;
	/** Max y point */
	const float maxY;

protected:
	/** Creates new are from given coords */
	RectangleArea(const world::zone::ZoneName* zoneName, int32_t worldId, float minX, float minY, float maxX, float maxY, float minZ, float maxZ);

public:
	/** @param zoneName nullable (ShieldService passes null) */
	static runtime::Ref<RectangleArea> create(const world::zone::ZoneName* zoneName, int32_t worldId, float minX, float minY, float maxX, float maxY,
		float minZ, float maxZ);

	bool isInside2D(float x, float y) override;
	using AbstractArea::isInside2D;

	bool isInside3D(float x, float y, float z) override;
	using AbstractArea::isInside3D;

	double getDistance2D(float x, float y) override;
	using AbstractArea::getDistance2D;

	double getDistance3D(float x, float y, float z) override;
	using AbstractArea::getDistance3D;

	/** The Java body is getClosestPoint2D (AbstractArea class comment) */
	std::optional<templates::zone::Point2D> getClosestPoint(float x, float y) override;
	using AbstractArea::getClosestPoint;

	templates::zone::Point2D getClosestPoint2D(float x, float y) override;

	/** Java: TODO Auto-generated method stub, always false */
	bool intersectsRectangle(RectangleArea& area) override;

protected:
	~RectangleArea() override;
};

} // namespace aion::gameserver::model::geometry
