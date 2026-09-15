#include "aion/gameserver/geoEngine/collision/bih/BIHTree.h"

#include <algorithm>
#include <limits>

#include "aion/gameserver/geoEngine/bounding/BoundingBox.h"
#include "aion/gameserver/geoEngine/collision/CollisionResult.h"
#include "aion/gameserver/geoEngine/collision/CollisionResults.h"
#include "aion/gameserver/geoEngine/collision/bih/BIHNode.h"
#include "aion/gameserver/geoEngine/math/FastMath.h"
#include "aion/gameserver/geoEngine/math/Matrix4f.h"
#include "aion/gameserver/geoEngine/math/Ray.h"
#include "aion/gameserver/geoEngine/math/Vector3f.h"
#include "aion/gameserver/geoEngine/scene/Mesh.h"
#include "aion/gameserver/geoEngine/utils/JavaMathFloat.h"
#include "aion/gameserver/geoEngine/utils/TempVars.h"
// keep last: Java float semantics for the expressions of this file
#include "aion/gameserver/geoEngine/math/StrictFp.h"

namespace aion::gameserver::geoEngine::collision::bih {

using bounding::BoundingBox;
using math::Vector3f;
using utils::TempVars;

BIHTree::BIHTree(scene::Mesh& meshValue) : mesh(meshValue) {
}

BIHTree::~BIHTree() = default;

runtime::Ref<BIHTree> BIHTree::create(scene::Mesh& meshValue) {
	return runtime::makeRef<BIHTree>(meshValue);
}

void BIHTree::construct() {
	int32_t numTris = mesh->getTriangleCount();
	runtime::Ptr<BoundingBox> meshBound = runtime::cast<BoundingBox>(mesh->getBound());
	root.set(createNode(0, numTris - 1, *meshBound, 0));
}

runtime::Ref<BoundingBox> BIHTree::createBox(int32_t l, int32_t r) {
	TempVars& vars = TempVars::get();
	TempVars::ReleaseGuard releaseOnException(vars);
	constexpr float INF = std::numeric_limits<float>::infinity();
	Vector3f& min = vars.vect1.set(INF, INF, INF);
	Vector3f& max = vars.vect2.set(-INF, -INF, -INF);

	Vector3f& v1 = vars.vect3;
	Vector3f& v2 = vars.vect4;
	Vector3f& v3 = vars.vect5;

	for (int32_t i = l; i <= r; i++) {
		getTriangle(i, v1, v2, v3);
		BoundingBox::checkMinMax(min, max, v1);
		BoundingBox::checkMinMax(min, max, v2);
		BoundingBox::checkMinMax(min, max, v3);
	}

	runtime::Ref<BoundingBox> bbox = BoundingBox::create(min, max);
	vars.release();
	return bbox;
}

int32_t BIHTree::sortTriangles(int32_t l, int32_t r, float split, int32_t axis) {
	int32_t pivot = l;
	int32_t j = r;

	TempVars& vars = TempVars::get();
	TempVars::ReleaseGuard releaseOnException(vars);
	Vector3f& v1 = vars.vect1;
	Vector3f& v2 = vars.vect2;
	Vector3f& v3 = vars.vect3;

	while (pivot <= j) {
		getTriangle(pivot, v1, v2, v3);
		v1.addLocal(v2).addLocal(v3).multLocal(math::FastMath::ONE_THIRD);
		if (v1.get(axis) > split) {
			mesh->swapTriangles(pivot, j);
			--j;
		} else {
			++pivot;
		}
	}

	vars.release();
	pivot = (pivot == l && j < pivot) ? j : pivot;
	return pivot;
}

void BIHTree::setMinMax(BoundingBox& bbox, bool doMin, int32_t axis, float value) {
	Vector3f min = bbox.getMin();
	Vector3f max = bbox.getMax();

	if (doMin)
		min.set(axis, value);
	else
		max.set(axis, value);

	bbox.setMinMax(min, max);
}

float BIHTree::getMinMax(BoundingBox& bbox, bool doMin, int32_t axis) {
	if (doMin)
		return bbox.getMin().get(axis);
	else
		return bbox.getMax().get(axis);
}

runtime::Ref<BIHNode> BIHTree::createNode(int32_t l, int32_t r, BoundingBox& nodeBbox, int32_t depth) {
	if ((r - l) < MAX_TRIS_PER_NODE || depth > MAX_TREE_DEPTH) {
		return BIHNode::create(l, r);
	}

	runtime::Ref<BoundingBox> currentBox = &nodeBbox == mesh->getBound().get() ? runtime::Ref<BoundingBox>(nodeBbox) : createBox(l, r);

	Vector3f exteriorExt = nodeBbox.getExtent();
	Vector3f interiorExt = currentBox->getExtent();
	exteriorExt.subtractLocal(interiorExt);

	int32_t axis = 0;
	if (exteriorExt.x > exteriorExt.y) {
		if (exteriorExt.x > exteriorExt.z)
			axis = 0;
		else
			axis = 2;
	} else {
		if (exteriorExt.y > exteriorExt.z)
			axis = 1;
		else
			axis = 2;
	}
	if (exteriorExt.equals(Vector3f::ZERO))
		axis = 0;

	float split = currentBox->getCenter().get(axis);
	int32_t pivot = sortTriangles(l, r, split, axis);
	if (pivot == l || pivot == r)
		pivot = (r + l) / 2;

	// If one of the partitions is empty, continue with recursion: same level but different bbox
	if (pivot < l) {
		// Only right
		runtime::Ref<BoundingBox> rbbox = BoundingBox::create(*currentBox);
		setMinMax(*rbbox, true, axis, split);
		return createNode(l, r, *rbbox, depth + 1);
	} else if (pivot > r) {
		// Only left
		runtime::Ref<BoundingBox> lbbox = BoundingBox::create(*currentBox);
		setMinMax(*lbbox, false, axis, split);
		return createNode(l, r, *lbbox, depth + 1);
	} else {
		// Build the node
		runtime::Ref<BIHNode> node = BIHNode::create(axis);

		// Left child
		runtime::Ref<BoundingBox> lbbox = BoundingBox::create(*currentBox);
		setMinMax(*lbbox, false, axis, split);

		// The left node right border is the plane most right
		node->setLeftPlane(getMinMax(*createBox(l, std::max(l, pivot - 1)), false, axis));
		node->setLeftChild(createNode(l, std::max(l, pivot - 1), *lbbox, depth + 1)); // Recursive call

		// Right Child
		runtime::Ref<BoundingBox> rbbox = BoundingBox::create(*currentBox);
		setMinMax(*rbbox, true, axis, split);
		// The right node left border is the plane most left
		node->setRightPlane(getMinMax(*createBox(pivot, r), true, axis));
		node->setRightChild(createNode(pivot, r, *rbbox, depth + 1)); // Recursive call

		return node;
	}
}

void BIHTree::getTriangle(int32_t index, Vector3f& v1, Vector3f& v2, Vector3f& v3) {
	mesh->getTriangle(index, v1, v2, v3);
}

int32_t BIHTree::collideWithRay(math::Ray& r, const math::Matrix4f& worldMatrix, bounding::BoundingVolume& worldBound, CollisionResults& results) {
	CollisionResults wbCollisions(results.getIntentions(), results.getInstanceId(), results.isOnlyFirst());
	worldBound.collideWith(r, wbCollisions);
	int32_t collisions = 0;
	// if worldBound contains ray origin and there are no collisions it means ray starts and ends inside worldBound
	if (wbCollisions.size() > 0 || worldBound.contains(r.getOrigin())) {
		float tMin = 0;
		float tMax = r.getLimit();
		if (wbCollisions.size() > 0) {
			tMin = wbCollisions.getClosestCollision()->getDistance();
			tMax = wbCollisions.getFarthestCollision()->getDistance();
			if (tMax <= 0)
				tMax = std::numeric_limits<float>::infinity();
			else if (tMin == tMax)
				tMin = 0;

			if (tMin <= 0)
				tMin = 0;

			if (r.getLimit() < std::numeric_limits<float>::infinity())
				tMax = utils::javaMin(tMax, r.getLimit());
		}

		// collisions += root.intersectBrute(r, worldMatrix, this, tMin, tMax, results);
		collisions += root->intersectWhere(r, worldMatrix, *this, tMin, tMax, results);
	}
	return collisions;
}

int32_t BIHTree::collideWith(math::Ray& other, const math::Matrix4f& worldMatrix, bounding::BoundingVolume& worldBound, CollisionResults& results) {
	return collideWithRay(other, worldMatrix, worldBound, results);
}

} // namespace aion::gameserver::geoEngine::collision::bih
