#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/model/geometry/Area.h"
#include "aion/gameserver/model/geometry/fwd.h"
#include "aion/gameserver/model/templates/zone/fwd.h"
#include "aion/gameserver/world/zone/fwd.h"

namespace aion::gameserver::model::geometry {

/**
 * C++: fieldmap K4 (RefCounted, protected non-final fields as Field<T>); the first implementor of Area with a runtime base in its hierarchy, so
 * it forwards retain/release (hub-headers.md §9.2). The deprecated 2D methods keep Java's constant results; every getClosestPoint returns null.
 *
 * @author MrPoke
 */
class SphereArea : public runtime::RefCounted, public Area {
	AION_MAKE_REF_FRIEND
protected:
	runtime::Field<float> x{};
	runtime::Field<float> y{};
	runtime::Field<float> z{};
	runtime::Field<float> r{};
	runtime::Field<int32_t> worldId{};
	runtime::Field<const world::zone::ZoneName*> zoneName{};

	SphereArea(const world::zone::ZoneName* zoneName, int32_t worldId, float x, float y, float z, float r);

public:
	static runtime::Ref<SphereArea> create(const world::zone::ZoneName* zoneName, int32_t worldId, float x, float y, float z, float r);

	/** C++ only: Ref<Area> retains the implementing object (hub-headers.md §9.2). */
	void retain() const noexcept override { runtime::RefCounted::retain(); }
	void release() const noexcept override { runtime::RefCounted::release(); }

	/** Java: @Deprecated, always false */
	bool isInside2D(const templates::zone::Point2D* point) override;

	/** Java: @Deprecated, always false */
	bool isInside2D(float x, float y) override;

	bool isInside3D(Point3D& point) override;

	bool isInside3D(float x, float y, float z) override;

	bool isInsideZ(Point3D& point) override;

	bool isInsideZ(float z) override;

	/** Java: @Deprecated, always 0 */
	double getDistance2D(const templates::zone::Point2D* point) override;

	/** Java: @Deprecated, always 0 */
	double getDistance2D(float x, float y) override;

	double getDistance3D(Point3D& point) override;

	double getDistance3D(float x, float y, float z) override;

	/** Java: @Deprecated, always null */
	std::optional<templates::zone::Point2D> getClosestPoint(const templates::zone::Point2D* point) override;

	/** Java: @Deprecated, always null */
	std::optional<templates::zone::Point2D> getClosestPoint(float x, float y) override;

	/** Java: always null */
	runtime::Ref<Point3D> getClosestPoint(Point3D& point) override;

	/** Java: always null */
	runtime::Ref<Point3D> getClosestPoint(float x, float y, float z) override;

	float getMinZ() override;

	float getMaxZ() override;

	bool intersectsRectangle(RectangleArea& area) override;

	int32_t getWorldId() override;

	const world::zone::ZoneName* getZoneName() override;

protected:
	~SphereArea() override;
};

} // namespace aion::gameserver::model::geometry
