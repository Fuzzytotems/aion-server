#pragma once

#include <span>

#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/geoEngine/bounding/BoundingVolume_Type.h"
#include "aion/gameserver/geoEngine/bounding/fwd.h"
#include "aion/gameserver/geoEngine/collision/Collidable.h"
#include "aion/gameserver/geoEngine/math/Vector3f.h"
#include "aion/gameserver/geoEngine/math/fwd.h"

namespace aion::gameserver::geoEngine::bounding {

/**
 * <code>BoundingVolume</code> defines an interface for dealing with containment of a collection of points.
 * <p>
 * S0c declaration header (docs/design/hub-headers.md §3.5): the world bound of a Spatial. RefCounted (fieldmap K4). C++ notes: `center` is a
 * value (Vector3f is a value type), so getCenter() returns a copy and setCenter copies (Java shares the vector); computeFromPoints(FloatBuffer)
 * takes the float values as a span.
 *
 * @author Mark Powell
 */
class BoundingVolume : public runtime::RefCounted, public collision::Collidable {
	AION_MAKE_REF_FRIEND
public:
	using Type = BoundingVolume_Type;

	runtime::Field<math::Vector3f> center{};

protected:
	BoundingVolume();
	explicit BoundingVolume(const math::Vector3f& center);
	~BoundingVolume() override;

public:
	/**
	 * getType returns the type of bounding volume this is.
	 */
	virtual Type getType() = 0;

	/**
	 * <code>transform</code> alters the location of the bounding volume by a rotation, translation and a scalar.
	 *
	 * @param store volume to store result in (null for a new one)
	 * @return the new bounding volume (store or a new object)
	 */
	virtual runtime::Ref<BoundingVolume> transform(const math::Matrix4f& trans, runtime::Ptr<BoundingVolume> store) = 0;

	/**
	 * <code>computeFromPoints</code> generates a bounding volume that encompasses a collection of points (Java: FloatBuffer).
	 */
	virtual void computeFromPoints(std::span<const float> points) = 0;

	/**
	 * <code>mergeLocal</code> combines two bounding volumes into a single bounding volume that contains both this bounding volume and the
	 * parameter volume. The result is stored locally.
	 */
	virtual runtime::Ptr<BoundingVolume> mergeLocal(runtime::Ptr<BoundingVolume> volume) = 0;

	/**
	 * <code>clone</code> creates a new BoundingVolume object containing the same data as this one.
	 *
	 * @param store where to store the cloned information. if null or wrong class, a new store is created.
	 */
	virtual runtime::Ref<BoundingVolume> clone(runtime::Ptr<BoundingVolume> store) = 0;

	math::Vector3f getCenter() const { return center.get(); }

	math::Vector3f getCenter(math::Vector3f& store);

	void setCenter(const math::Vector3f& newCenter) { center.set(newCenter); }

	/**
	 * Find the distance from the center of this Bounding Volume to the given point.
	 */
	float distanceTo(const math::Vector3f& point);

	/**
	 * Find the squared distance from the center of this Bounding Volume to the given point.
	 */
	float distanceSquaredTo(const math::Vector3f& point);

	/**
	 * Find the distance from the nearest edge of this Bounding Volume to the given point.
	 */
	virtual float distanceToEdge(const math::Vector3f& point) = 0;

	/**
	 * determines if this bounding volume and a second given volume are intersecting.
	 */
	virtual bool intersects(BoundingVolume& bv) = 0;

	/**
	 * determines if a ray intersects this bounding volume.
	 */
	virtual bool intersects(const math::Ray& ray) = 0;

	/**
	 * determines if this bounding volume and a given bounding box are intersecting.
	 */
	virtual bool intersectsBoundingBox(BoundingBox& bb) = 0;

	/**
	 * determines if a given point is contained within this bounding volume.
	 */
	virtual bool contains(const math::Vector3f& point) = 0;

	/**
	 * Determines if a given point intersects (touches or is inside) this bounding volume.
	 */
	virtual bool intersects(const math::Vector3f& point) = 0;

	virtual float getVolume() = 0;
};

} // namespace aion::gameserver::geoEngine::bounding
