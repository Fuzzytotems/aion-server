#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/geoEngine/bounding/fwd.h"
#include "aion/gameserver/geoEngine/collision/bih/fwd.h"
#include "aion/gameserver/geoEngine/collision/fwd.h"
#include "aion/gameserver/geoEngine/math/fwd.h"
#include "aion/gameserver/geoEngine/scene/CollisionData.h"
#include "aion/gameserver/geoEngine/scene/fwd.h"

namespace aion::gameserver::geoEngine::collision::bih {

/**
 * Bounding Interval Hierarchy of the triangles of one Mesh (jMonkeyEngine). construct() sorts the mesh's triangles in place.
 * <p>
 * RefCounted (fieldmap K4), created with create(mesh). The mesh ↔ tree reference cycle is accepted in cycles.toml (server lifetime).
 */
class BIHTree : public runtime::RefCounted, public scene::CollisionData {
	AION_MAKE_REF_FRIEND
public:
	static constexpr int32_t MAX_TREE_DEPTH = 100;
	static constexpr int32_t MAX_TRIS_PER_NODE = 21;

private:
	runtime::Field<runtime::Ref<BIHNode>> root{};
	const runtime::Ref<scene::Mesh> mesh;

protected:
	explicit BIHTree(scene::Mesh& mesh);
	~BIHTree() override;

public:
	void retain() const noexcept override { runtime::RefCounted::retain(); }
	void release() const noexcept override { runtime::RefCounted::release(); }

	/** Java: new BIHTree(mesh) */
	static runtime::Ref<BIHTree> create(scene::Mesh& mesh);

	/** @throws ClassCastException if the mesh bound is not a BoundingBox */
	void construct();

private:
	runtime::Ref<bounding::BoundingBox> createBox(int32_t l, int32_t r);

	int32_t sortTriangles(int32_t l, int32_t r, float split, int32_t axis);

	void setMinMax(bounding::BoundingBox& bbox, bool doMin, int32_t axis, float value);

	float getMinMax(bounding::BoundingBox& bbox, bool doMin, int32_t axis);

	runtime::Ref<BIHNode> createNode(int32_t l, int32_t r, bounding::BoundingBox& nodeBbox, int32_t depth);

public:
	void getTriangle(int32_t index, math::Vector3f& v1, math::Vector3f& v2, math::Vector3f& v3);

private:
	int32_t collideWithRay(math::Ray& r, const math::Matrix4f& worldMatrix, bounding::BoundingVolume& worldBound, CollisionResults& results);

public:
	int32_t collideWith(math::Ray& other, const math::Matrix4f& worldMatrix, bounding::BoundingVolume& worldBound, CollisionResults& results) override;

	/** C++ only (tests): the root node, null before construct() */
	runtime::Ptr<BIHNode> getRoot() const { return root.get(); }
};

} // namespace aion::gameserver::geoEngine::collision::bih
