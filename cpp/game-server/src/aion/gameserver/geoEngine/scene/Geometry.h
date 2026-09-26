#pragma once

#include <cstdint>
#include <string_view>

#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/geoEngine/bounding/fwd.h"
#include "aion/gameserver/geoEngine/collision/fwd.h"
#include "aion/gameserver/geoEngine/math/Matrix4f.h"
#include "aion/gameserver/geoEngine/math/fwd.h"
#include "aion/gameserver/geoEngine/scene/Spatial.h"
#include "aion/gameserver/geoEngine/scene/fwd.h"

namespace aion::gameserver::geoEngine::scene {

/**
 * A leaf of the scene graph: one placement of a Mesh with its world matrix and world bound.
 * <p>
 * RefCounted (fieldmap K4, the class tree of Spatial), created with create(). C++ notes: `cachedWorldMat` is a Field of the Matrix4f value, so
 * getWorldMatrix() returns a copy (Java returns the live matrix, which only setTransform changes); Java's covariant `Geometry clone()` keeps the
 * base's `Ref<Spatial>` return (hub-headers.md §8.2) and, like Object.clone, copies the name, mesh, world bound and parent link and gives the
 * copy its own matrix. `final` (no Java subclass) lets runtime::as/cast use the typeid fast path in Node::collideWith.
 */
class Geometry final : public Spatial {
	AION_MAKE_REF_FRIEND
protected:
	/** The mesh contained herein */
	runtime::Field<runtime::Ref<Mesh>> mesh{};

	runtime::Field<math::Matrix4f> cachedWorldMat{};

	/**
	 * Do not use this constructor. Serialization purposes only.
	 */
	Geometry();

	/**
	 * Create a geometry node with mesh data.
	 *
	 * @param name The name of this geometry
	 * @param mesh The mesh data for this geometry
	 * @throws NullPointerException if mesh is null
	 */
	Geometry(std::string_view name, runtime::Ptr<Mesh> mesh);

	~Geometry() override;

public:
	/** Java: new Geometry(name, mesh) */
	static runtime::Ref<Geometry> create(std::string_view name, runtime::Ptr<Mesh> mesh);

	int32_t getVertexCount() override;

	int32_t getTriangleCount() override;

	void setMesh(runtime::Ptr<Mesh> mesh);

	runtime::Ptr<Mesh> getMesh() const { return mesh.get(); }

	/**
	 * @return The bounding volume of the mesh, in model space.
	 */
	runtime::Ptr<bounding::BoundingVolume> getModelBound();

	/**
	 * Updates the bounding volume of the mesh. Should be called when the mesh has been modified.
	 */
	void updateModelBound() override;

	/** Java: Matrix4f getWorldMatrix() (a copy of the value) */
	math::Matrix4f getWorldMatrix() const { return cachedWorldMat.get(); }

	void setModelBound(runtime::Ptr<bounding::BoundingVolume> modelBound) override;

	int32_t collideWith(math::Ray& other, collision::CollisionResults& results) override;

	void setTransform(const math::Matrix3f& rotation, const math::Vector3f& loc, const math::Vector3f& scale) override;

	int8_t getCollisionIntentions() override;

	void setCollisionIntentions(int8_t collisionIntentions) override;

	int32_t getMaterialId() override;

	void setMaterialId(int8_t materialId) override;

	/** Java: Geometry clone() (covariant return) */
	runtime::Ref<Spatial> clone() override;
};

} // namespace aion::gameserver::geoEngine::scene
