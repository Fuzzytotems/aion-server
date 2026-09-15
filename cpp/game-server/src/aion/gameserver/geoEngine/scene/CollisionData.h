#pragma once

#include <cstdint>

#include "aion/gameserver/geoEngine/bounding/fwd.h"
#include "aion/gameserver/geoEngine/collision/fwd.h"
#include "aion/gameserver/geoEngine/math/fwd.h"
#include "aion/gameserver/geoEngine/scene/fwd.h"

namespace aion::gameserver::geoEngine::scene {

/**
 * <code>CollisionData</code> is an interface that can be used to do triangle-accurate collision between bounding volumes and rays.
 * <p>
 * Interface held by `Ref<CollisionData>` (Mesh.collisionTree), so it declares the reference count operations (hub-headers.md §9.2). Like
 * Collidable, the `other` parameter is a math::Ray (every Java caller passes one).
 *
 * @author Kirill Vainer
 */
class CollisionData {
public:
	/** C++ only: Ref<CollisionData> retains the implementing object (hub-headers.md §9.2). */
	virtual void retain() const noexcept = 0;
	virtual void release() const noexcept = 0;

	virtual int32_t collideWith(math::Ray& other, const math::Matrix4f& worldMatrix, bounding::BoundingVolume& worldBound,
		collision::CollisionResults& results) = 0;

	virtual ~CollisionData() = default;

protected:
	CollisionData() = default;
	CollisionData(const CollisionData&) = default;
	CollisionData& operator=(const CollisionData&) = default;
};

} // namespace aion::gameserver::geoEngine::scene
