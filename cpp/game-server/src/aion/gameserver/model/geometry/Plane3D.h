#pragma once

#include <optional>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/geoEngine/math/Vector3f.h"
#include "aion/gameserver/model/geometry/fwd.h"

namespace aion::gameserver::model::geometry {

/**
 * C++: fieldmap K3 (immutable, base RefCounted: FlyRing.plane, Road.plane); created with create(p1, p2, p3). intersection returns
 * std::optional<Vector3f> (Java: a new Vector3f or null).
 *
 * @author Neon
 */
class Plane3D : public runtime::RefCounted {
	AION_MAKE_REF_FRIEND
private:
	const geoEngine::math::Vector3f pointOnPlane;
	const geoEngine::math::Vector3f normal;

protected:
	Plane3D(const geoEngine::math::Vector3f& p1, const geoEngine::math::Vector3f& p2, const geoEngine::math::Vector3f& p3);

public:
	static runtime::Ref<Plane3D> create(const geoEngine::math::Vector3f& p1, const geoEngine::math::Vector3f& p2,
		const geoEngine::math::Vector3f& p3);

	/** @return the intersection of the ray segment with the plane, null (std::nullopt) if it is parallel or outside the segment */
	std::optional<geoEngine::math::Vector3f> intersection(const geoEngine::math::Vector3f& rayStart, const geoEngine::math::Vector3f& rayEnd) const;

private:
	/** C++ only: Java's constructor computes normal from vector1.cross(vector2) before storing it */
	static geoEngine::math::Vector3f normalOf(const geoEngine::math::Vector3f& p1, const geoEngine::math::Vector3f& p2,
		const geoEngine::math::Vector3f& p3) noexcept;

protected:
	~Plane3D() override;
};

} // namespace aion::gameserver::model::geometry
