#pragma once

#include <cstdint>
#include <optional>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/model/geometry/Area.h"
#include "aion/gameserver/model/geometry/fwd.h"
#include "aion/gameserver/model/templates/zone/fwd.h"
#include "aion/gameserver/world/zone/fwd.h"

namespace aion::gameserver::model::geometry {

/**
 * Class with basic method implementation for ares.<br>
 * If possible it should be subclassed. <br>
 * In other case {@link com.aionemu.gameserver.model.geometry.Area} should be implemented directly
 * <p>
 * C++: the first implementor of Area with a runtime base (fieldmap: K3 immutable, base RefCounted), so it forwards retain/release
 * (hub-headers.md §9.2). Java's getClosestPoint overloads return new points: the Point3D overloads return a new `runtime::Ref<Point3D>`, the
 * Point2D overloads the point by value (`std::optional`, header requests base-1/base-2). The areas compute their closest points through the
 * C++-only getClosestPoint2D, which returns the (never null) point by value; their getClosestPoint(float, float) overrides return it.
 *
 * @author SoulKeeper
 */
class AbstractArea : public runtime::RefCounted, public Area {
private:
	/** Minimal z of area */
	const float minZ;
	/** Maximal Z of area */
	const float maxZ;
	const world::zone::ZoneName* zoneName;
	const int32_t worldId;

protected:
	/**
	 * Creates new AbstractArea with min and max z
	 *
	 * @throws IllegalArgumentException
	 *           if minZ > maxZ
	 */
	AbstractArea(const world::zone::ZoneName* zoneName, int32_t worldId, float minZ, float maxZ);

public:
	/** C++ only: Ref<Area> retains the implementing object (hub-headers.md §9.2). */
	void retain() const noexcept override { runtime::RefCounted::retain(); }
	void release() const noexcept override { runtime::RefCounted::release(); }

	bool isInside2D(const templates::zone::Point2D* point) override;
	using Area::isInside2D;

	bool isInside3D(Point3D& point) override;

	bool isInside3D(float x, float y, float z) override;

	bool isInsideZ(Point3D& point) override;

	bool isInsideZ(float z) override;

	double getDistance2D(const templates::zone::Point2D* point) override;
	using Area::getDistance2D;

	double getDistance3D(Point3D& point) override;
	using Area::getDistance3D;

	std::optional<templates::zone::Point2D> getClosestPoint(const templates::zone::Point2D* point) override;

	/** @return a new point */
	runtime::Ref<Point3D> getClosestPoint(Point3D& point) override;

	/** @return a new point */
	runtime::Ref<Point3D> getClosestPoint(float x, float y, float z) override;
	using Area::getClosestPoint;

	/**
	 * C++ only: Java getClosestPoint(float x, float y) of the subclass, returning the new point by value (see the class comment). The subclasses'
	 * Java bodies are ported here; their getClosestPoint(float, float) overrides return it.
	 */
	virtual templates::zone::Point2D getClosestPoint2D(float x, float y) = 0;

	float getMinZ() override { return minZ; }

	float getMaxZ() override { return maxZ; }

	int32_t getWorldId() override { return worldId; }

	/** @return the zoneName */
	const world::zone::ZoneName* getZoneName() override { return zoneName; }

protected:
	~AbstractArea() override;
};

} // namespace aion::gameserver::model::geometry
