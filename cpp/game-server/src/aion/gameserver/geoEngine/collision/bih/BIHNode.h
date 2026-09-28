#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/geoEngine/collision/bih/fwd.h"
#include "aion/gameserver/geoEngine/collision/fwd.h"
#include "aion/gameserver/geoEngine/math/fwd.h"

namespace aion::gameserver::geoEngine::collision::bih {

/**
 * Bounding Interval Hierarchy. Based on: Instant Ray Tracing: The Bounding Interval Hierarchy By Carsten Wächter and Alexander Keller
 * <p>
 * A node of a BIHTree. RefCounted (fieldmap K4), created with create(). The tree is built once by BIHTree::construct before the mesh is used
 * for collisions (GeoWorldLoader builds every tree eagerly) and never changes afterwards.
 * <p>
 * C++ notes: BIHStackData, the traversal stack entry that Java allocates per push into the thread-local TempVars.bihStack, is a value struct
 * holding a Ref to the node (fieldmap.toml K5, header request geo-3: a heap object per push would put every ray query on the Reclaimer).
 * intersectWhere empties the stack before it returns or throws, so no Ref stays in the thread-local storage between calls (L15).
 * intersectWhere transforms the ray's own origin and direction in place and restores them before it returns, like Java.
 */
class BIHNode : public runtime::RefCounted {
	AION_MAKE_REF_FRIEND
public:
	/** Java: public static final class BIHStackData */
	// fieldmap-class: com.aionemu.gameserver.geoEngine.collision.bih.BIHNode.BIHStackData
	struct BIHStackData {
		// fieldmap.toml: K5, an element of the thread-local traversal stack (TempVars.bihStack) of one ray query (header request geo-3)
		const runtime::Ref<BIHNode> node;
		const float min;
		const float max;

		BIHStackData(runtime::Ref<BIHNode> node, float min, float max);
	};

private:
	const int32_t leftIndex;
	const int32_t rightIndex;
	runtime::Field<runtime::Ref<BIHNode>> left{};
	runtime::Field<runtime::Ref<BIHNode>> right{};
	runtime::Field<float> leftPlane{};
	runtime::Field<float> rightPlane{};
	const int32_t axis;

protected:
	/** Java: BIHNode(int l, int r) - a leaf (axis 3) */
	BIHNode(int32_t l, int32_t r);
	/** Java: BIHNode(int axis) */
	explicit BIHNode(int32_t axis);
	/** Java: BIHNode() */
	BIHNode();
	~BIHNode() override;

public:
	/** Java: new BIHNode(l, r) */
	static runtime::Ref<BIHNode> create(int32_t l, int32_t r);
	/** Java: new BIHNode(axis) */
	static runtime::Ref<BIHNode> create(int32_t axis);
	/** Java: new BIHNode() */
	static runtime::Ref<BIHNode> create();

	runtime::Ptr<BIHNode> getLeftChild() const { return left.get(); }

	void setLeftChild(runtime::Ptr<BIHNode> left);

	float getLeftPlane() const { return leftPlane.get(); }

	void setLeftPlane(float value) { leftPlane.set(value); }

	runtime::Ptr<BIHNode> getRightChild() const { return right.get(); }

	void setRightChild(runtime::Ptr<BIHNode> right);

	float getRightPlane() const { return rightPlane.get(); }

	void setRightPlane(float value) { rightPlane.set(value); }

	/** C++ only (tests): the axis (3 for a leaf) and the triangle index range of a leaf */
	int32_t getAxis() const { return axis; }
	int32_t getLeftIndex() const { return leftIndex; }
	int32_t getRightIndex() const { return rightIndex; }

	/**
	 * Collides the ray, transformed into the mesh's model space, with the triangles of this subtree between the ray parameters sceneMin and
	 * sceneMax and adds world space collisions to results.
	 *
	 * @return the number of collisions added
	 */
	int32_t intersectWhere(math::Ray& r, const math::Matrix4f& worldMatrix, BIHTree& tree, float sceneMin, float sceneMax, CollisionResults& results);
};

} // namespace aion::gameserver::geoEngine::collision::bih
