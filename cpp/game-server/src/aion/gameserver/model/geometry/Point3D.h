#pragma once

#include <cstdint>
#include <string>

#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/model/geometry/fwd.h"
#include "aion/gameserver/model/templates/zone/fwd.h"

namespace aion::gameserver::model::geometry {

/**
 * This class represents 3D point.<br>
 * It's valid for serializing and cloning.
 * <p>
 * S0c declaration header (docs/design/hub-headers.md §3.5): RefCounted (fieldmap K4, NpcMoveController.lastSteps); Java Cloneable and Serializable
 * (clone() returns a new Ref).
 *
 * @author SoulKeeper
 */
class Point3D : public runtime::RefCounted {
	AION_MAKE_REF_FRIEND
private:
	runtime::Field<float> x{};
	runtime::Field<float> y{};
	runtime::Field<float> z{};

protected:
	/** Creates new point with coords 0, 0, 0 */
	Point3D();

public:
	static runtime::Ref<Point3D> create();

protected:
	/** Creates new 3D point from 2D point and z coord */
	Point3D(const templates::zone::Point2D* point, float z);

public:
	static runtime::Ref<Point3D> create(const templates::zone::Point2D* point, float value);

protected:
	/** Clones another 3D point */
	explicit Point3D(Point3D& point);

public:
	static runtime::Ref<Point3D> create(Point3D& point);

protected:
	/** Creates new 3d point with given coords */
	Point3D(float x, float y, float z);

public:
	static runtime::Ref<Point3D> create(float value, float yValue, float zValue);

protected:
	Point3D(double x, double y, double z);

public:
	static runtime::Ref<Point3D> create(double value, double yValue, double zValue);

	/** Returns x coord */
	float getX() const { return this->x.get(); }

	/** Sets x coord of this point */
	void setX(float value) { this->x.set(value); }

	/** Returns y coord of this point */
	float getY() const { return this->y.get(); }

	/** Sets y coord of this point */
	void setY(float value) { this->y.set(value); }

	/** Returns z coord of this point */
	float getZ() const { return this->z.get(); }

	/** Sets z coord of this point */
	void setZ(float value) { this->z.set(value); }

	/** Checks if this point is equal to another point */
	bool equals(const Point3D& o) const;

	/** Returns point's hashcode.<br> */
	int32_t hashCode() const;

	/** Clones this point */
	runtime::Ref<Point3D> clone();

	/** Formatted string representation of this point */
	std::string toString();

protected:
	~Point3D() override;
};

} // namespace aion::gameserver::model::geometry
