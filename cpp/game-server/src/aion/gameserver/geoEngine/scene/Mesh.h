#pragma once

#include <cstdint>
#include <span>

#include "aion/gameserver/runtime/fields/Array.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/geoEngine/bounding/fwd.h"
#include "aion/gameserver/geoEngine/collision/fwd.h"
#include "aion/gameserver/geoEngine/math/fwd.h"
#include "aion/gameserver/geoEngine/scene/fwd.h"
#include "aion/gameserver/geoEngine/scene/mesh/fwd.h"

namespace aion::gameserver::geoEngine::scene {

/**
 * The triangles of one model of models.mesh: vertices, triangle indices, the model bound, the BIH collision tree, material and collision
 * intentions. Shared by every Geometry that places the model.
 * <p>
 * RefCounted (fieldmap K4), created with create(). C++ notes: setVertices/setIndices take the values instead of Java's FloatBuffer/Buffer
 * (copied like Java); getBound/setBound use Ptr (Node.setModelBound passes null). createCollisionData is run eagerly by GeoWorldLoader for every
 * mesh of the loaded maps before the maps are published (DEVIATIONS: Java builds the trees lazily and concurrently).
 */
class Mesh : public runtime::RefCounted {
	AION_MAKE_REF_FRIEND
private:
	/** The bounding volume that contains the mesh entirely. By default a BoundingBox (AABB). */
	runtime::Field<runtime::Ref<bounding::BoundingVolume>> meshBound{};

	runtime::Field<runtime::Ref<CollisionData>> collisionTree{};

	runtime::Field<runtime::Ref<runtime::Array<float>>> vertices{};
	runtime::Field<runtime::Ref<mesh::IndexArray>> indices{};

	runtime::Field<int8_t> materialId{0};
	runtime::Field<int8_t> collisionIntentions{0};

protected:
	Mesh();
	~Mesh() override;

public:
	/** Java: new Mesh() */
	static runtime::Ref<Mesh> create();

	int32_t getTriangleCount();

	/** Java: the number of vertex coordinates (vertices.length, three per vertex) */
	int32_t getVertexCount();

	void getTriangle(int32_t index, math::Vector3f& v1, math::Vector3f& v2, math::Vector3f& v3);

	void swapTriangles(int32_t i1, int32_t i2);

	/** Builds the BIH tree once (no-op if it exists). */
	void createCollisionData();

	int32_t collideWith(math::Ray& other, const math::Matrix4f& worldMatrix, bounding::BoundingVolume& worldBound, collision::CollisionResults& results);

	/** Java: setVertices(FloatBuffer) - copies the values and drops the collision tree */
	void setVertices(std::span<const float> vertices);

	/** Java: setIndices(ByteBuffer) - copies the bytes and drops the collision tree */
	void setIndices(std::span<const int8_t> indices);

	/** Java: setIndices(ShortBuffer) - copies the shorts and drops the collision tree */
	void setIndices(std::span<const int16_t> indices);

	void updateBound();

	runtime::Ptr<bounding::BoundingVolume> getBound() const { return meshBound.get(); }

	void setBound(runtime::Ptr<bounding::BoundingVolume> modelBound);

	void setCollisionIntentions(int8_t value) { collisionIntentions.set(value); }

	void setMaterialId(int8_t value) { materialId.set(value); }

	int8_t getCollisionIntentions() const { return collisionIntentions.get(); }

	int32_t getMaterialId() const { return materialId.get() & 0xFF; }

	/** C++ only (tests, eager build): the collision tree, null until createCollisionData */
	runtime::Ptr<CollisionData> getCollisionTree() const { return collisionTree.get(); }
};

} // namespace aion::gameserver::geoEngine::scene
