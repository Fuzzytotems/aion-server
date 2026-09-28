#pragma once

#include <cstdint>
#include <span>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/geometry/AbstractArea.h"
#include "aion/gameserver/model/geometry/fwd.h"
#include "aion/gameserver/model/templates/zone/fwd.h"
#include "aion/gameserver/world/zone/fwd.h"

namespace aion::gameserver::model::geometry {

/**
 * Area of free form
 * <p>
 * C++: fieldmap K4 (RefCounted through AbstractArea); created with create(...). Java's two constructors (Collection<Point2D> and Point2D[])
 * take one `std::span<const Point2D>` (the zone templates hold the points in a vector). getClosestPoint(float, float) is ported as
 * getClosestPoint2D (AbstractArea class comment).
 *
 * @author SoulKeeper
 */
class PolyArea : public AbstractArea {
	AION_MAKE_REF_FRIEND
private:
	/** Polygon used to calculate isInside() */
	const runtime::Ref<Polygon2D> poly;

protected:
	/**
	 * Creates new area from given points
	 *
	 * @throws IllegalArgumentException
	 *           if there are less than 3 points
	 */
	PolyArea(const world::zone::ZoneName* zoneName, int32_t worldId, std::span<const templates::zone::Point2D> points, float zMin, float zMax);

public:
	static runtime::Ref<PolyArea> create(const world::zone::ZoneName* zoneName, int32_t worldId, std::span<const templates::zone::Point2D> points,
		float zMin, float zMax);

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

private:
	/** C++ only: the Polygon2D Java builds in the constructor body after the point count check */
	static runtime::Ref<Polygon2D> newPolygon(std::span<const templates::zone::Point2D> points);

protected:
	~PolyArea() override;
};

} // namespace aion::gameserver::model::geometry
