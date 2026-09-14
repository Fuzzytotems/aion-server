#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/geometry/fwd.h"
#include "aion/gameserver/model/templates/zone/fwd.h"
#include "aion/gameserver/world/zone/fwd.h"

namespace aion::gameserver::model::geometry {

/**
 * Basic interface for all areas in AionEmu.<br>
 * It should be implemented in different ways for performance reasons.<br>
 * For instance, we don't need complex math for squares or circles, but we need it for more complex polygons.
 * <p>
 * S0c declaration header (docs/design/hub-headers.md §3.5), written for the member-type guards waiting for it at the spine freeze.
 *
 * @author SoulKeeper
 */
class Area {
public:
	/** Returns true if point is inside area ignoring z value */
	virtual bool isInside2D(const templates::zone::Point2D* point) = 0;

	/** Returns true if coords are inside area ignoring z value */
	virtual bool isInside2D(float x, float y) = 0;

	/** Returns true if point is inside area */
	virtual bool isInside3D(Point3D& point) = 0;

	/** Returns true if coors are inside area */
	virtual bool isInside3D(float x, float y, float z) = 0;

	/** Checks if z coord is insize */
	virtual bool isInsideZ(Point3D& point) = 0;

	/** Checks is z coord is inside */
	virtual bool isInsideZ(float z) = 0;

	/**
	 * Returns distance from point to closest point of this area ignoring z.<br>
	 * Returns 0 if point is inside area.
	 */
	virtual double getDistance2D(const templates::zone::Point2D* point) = 0;

	/**
	 * Returns distance from point to closest point of this area ignoring z.<br>
	 * Returns 0 point is inside area.
	 */
	virtual double getDistance2D(float x, float y) = 0;

	/**
	 * Returns distance from point to this area.<br>
	 * Returns 0 if is inside.
	 */
	virtual double getDistance3D(Point3D& point) = 0;

	/** Returns distance from coords to this area */
	virtual double getDistance3D(float x, float y, float z) = 0;

	/**
	 * Returns closest point of area to given point.<br>
	 * Returns point with coords = point arg if is inside
	 */
	virtual const templates::zone::Point2D* getClosestPoint(const templates::zone::Point2D* point) = 0;

	/**
	 * Returns closest point of area to given coords.<br>
	 * Returns point with coords x and y if coords are inside
	 */
	virtual const templates::zone::Point2D* getClosestPoint(float x, float y) = 0;

	/**
	 * Returns closest point of area to given point.<br>
	 * Works exactly like {@link #getClosestPoint(int, int)} if {@link #isInsideZ(int)} returns true.<br>
	 * In other case closest z edge is set as z coord.
	 */
	virtual runtime::Ptr<Point3D> getClosestPoint(Point3D& point) = 0;

	/**
	 * Returns closest point of area to given coords.<br>
	 * Works exactly like {@link #getClosestPoint(int, int)} if {@link #isInsideZ(int)} returns true.<br>
	 * In other case closest z edge is set as z coord.
	 */
	virtual runtime::Ptr<Point3D> getClosestPoint(float x, float y, float z) = 0;

	/** Return minimal z of this area */
	virtual float getMinZ() = 0;

	/** Returns maximal z of this area */
	virtual float getMaxZ() = 0;

	virtual bool intersectsRectangle(RectangleArea& area) = 0;

	virtual int32_t getWorldId() = 0;

	virtual const world::zone::ZoneName* getZoneName() = 0;

	/** C++ only: Ref<Area> retains the implementing object (hub-headers.md §9.2). */
	virtual void retain() const noexcept = 0;

	virtual void release() const noexcept = 0;

	virtual ~Area() = default;
};

} // namespace aion::gameserver::model::geometry
