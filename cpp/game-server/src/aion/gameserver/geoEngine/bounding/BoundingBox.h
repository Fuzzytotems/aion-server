#pragma once

#include <cstdint>
#include <span>
#include <string>

#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/geoEngine/bounding/BoundingVolume.h"
#include "aion/gameserver/geoEngine/bounding/fwd.h"
#include "aion/gameserver/geoEngine/collision/fwd.h"
#include "aion/gameserver/geoEngine/math/Vector3f.h"
#include "aion/gameserver/geoEngine/math/fwd.h"

namespace aion::gameserver::geoEngine::bounding {

/**
 * <code>BoundingBox</code> defines an axis-aligned cube that defines a container for a group of vertices of a particular piece of geometry.
 * This box defines a center and extents from that center along the x, y and z axis.
 * <p>
 * RefCounted (fieldmap K4, the class tree of Spatial.worldBound), created with create(). C++ notes: Java's covariant `BoundingBox clone(store)`
 * keeps the base's `Ref<BoundingVolume>` return (hub-headers.md §8.2); getMin/getMax/getExtent with a null store are the overloads without a
 * store; `center` is the base's Field, so the Java in-place updates of `center.x` are a load, a change and a store.
 *
 * @author Joshua Slack
 */
class BoundingBox final : public BoundingVolume {
	AION_MAKE_REF_FRIEND
public:
	runtime::Field<float> xExtent{};
	runtime::Field<float> yExtent{};
	runtime::Field<float> zExtent{};

protected:
	/** Default constructor instantiates a new <code>BoundingBox</code> object. */
	BoundingBox();
	/** Contstructor instantiates a new <code>BoundingBox</code> object with given specs. */
	BoundingBox(const math::Vector3f& c, float x, float y, float z);
	explicit BoundingBox(BoundingBox& source);
	BoundingBox(const math::Vector3f& min, const math::Vector3f& max);
	~BoundingBox() override;

public:
	/** Java: new BoundingBox() */
	static runtime::Ref<BoundingBox> create();
	/** Java: new BoundingBox(c, x, y, z) */
	static runtime::Ref<BoundingBox> create(const math::Vector3f& c, float x, float y, float z);
	/** Java: new BoundingBox(source) */
	static runtime::Ref<BoundingBox> create(BoundingBox& source);
	/** Java: new BoundingBox(min, max) */
	static runtime::Ref<BoundingBox> create(const math::Vector3f& min, const math::Vector3f& max);

	Type getType() override;

	/**
	 * <code>computeFromPoints</code> creates a new Bounding Box from a given set of points. It uses the <code>containAABB</code> method as
	 * default.
	 */
	void computeFromPoints(std::span<const float> points) override;

	static void checkMinMax(math::Vector3f& min, math::Vector3f& max, const math::Vector3f& point);

	/**
	 * <code>containAABB</code> creates a minimum-volume axis-aligned bounding box of the points (Java: FloatBuffer, null is ignored).
	 *
	 * @throws IllegalArgumentException for fewer than 3 values
	 */
	void containAABB(std::span<const float> points);

	runtime::Ref<BoundingVolume> transform(const math::Matrix4f& trans, runtime::Ptr<BoundingVolume> store) override;

	/**
	 * <code>mergeLocal</code> combines this box with a second bounding volume locally.
	 *
	 * @return this (null for a volume that is not an AABB)
	 */
	runtime::Ptr<BoundingVolume> mergeLocal(runtime::Ptr<BoundingVolume> volume) override;

private:
	/** <code>mergeLocal</code> combines this bounding box locally with a second bounding box described by its center and extents. */
	BoundingBox& mergeLocal(const math::Vector3f& boxCenter, float boxX, float boxY, float boxZ);

public:
	/** Java: BoundingBox clone(BoundingVolume store) */
	runtime::Ref<BoundingVolume> clone(runtime::Ptr<BoundingVolume> store) override;

	std::string toString();

	/** intersects determines if this Bounding Box intersects with another given bounding volume. */
	bool intersects(BoundingVolume& bv) override;

	/** determines if this bounding box intersects a given bounding box. */
	bool intersectsBoundingBox(BoundingBox& bb) override;

	/** determines if this bounding box intersects with a given ray object. */
	bool intersects(const math::Ray& ray) override;

private:
	int32_t collideWithRay(const math::Ray& ray, collision::CollisionResults& results);

public:
	int32_t collideWith(math::Ray& other, collision::CollisionResults& results) override;

	bool contains(const math::Vector3f& point) override;

	bool intersects(const math::Vector3f& point) override;

	float distanceToEdge(const math::Vector3f& point) override;

private:
	/** <code>clip</code> determines if a line segment intersects the current test plane. */
	bool clip(float denom, float numer, std::span<float, 3> t);

public:
	/** Query extent (Java: getExtent(null)). */
	math::Vector3f getExtent();
	math::Vector3f& getExtent(math::Vector3f& store);

	float getXExtent() const { return xExtent.get(); }

	float getYExtent() const { return yExtent.get(); }

	float getZExtent() const { return zExtent.get(); }

	/** @throws IllegalArgumentException for a negative extent */
	void setXExtent(float xExtent);

	/** @throws IllegalArgumentException for a negative extent */
	void setYExtent(float yExtent);

	/** @throws IllegalArgumentException for a negative extent */
	void setZExtent(float zExtent);

	/** Java: getMin(null) */
	math::Vector3f getMin();
	math::Vector3f& getMin(math::Vector3f& store);

	/** Java: getMax(null) */
	math::Vector3f getMax();
	math::Vector3f& getMax(math::Vector3f& store);

	void setMinMax(const math::Vector3f& min, const math::Vector3f& max);

	float getVolume() override;
};

} // namespace aion::gameserver::geoEngine::bounding
