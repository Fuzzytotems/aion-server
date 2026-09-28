#include "aion/gameserver/geoEngine/collision/bih/BIHTree.h"

#include <algorithm>
#include <limits>
#include <string>
#include <utility>
#include <vector>

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

namespace {

/**
 * The center and extents of a BoundingBox as a value, with the expressions of BoundingBox.cpp (setMinMax, getMin, getMax, getExtent), so the
 * results are bit-identical to the boxes the Java-shaped members create.
 */
struct BoxValue {
	Vector3f center;
	float xExtent = 0;
	float yExtent = 0;
	float zExtent = 0;

	static BoxValue of(BoundingBox& box) {
		BoxValue value;
		value.center = box.getCenter();
		Vector3f extent = box.getExtent();
		value.xExtent = extent.x;
		value.yExtent = extent.y;
		value.zExtent = extent.z;
		return value;
	}

	void setMinMax(const Vector3f& min, const Vector3f& max) {
		Vector3f c = Vector3f(max).addLocal(min).multLocal(0.5f);
		center = c;
		xExtent = math::FastMath::abs(max.x - c.x);
		yExtent = math::FastMath::abs(max.y - c.y);
		zExtent = math::FastMath::abs(max.z - c.z);
	}

	Vector3f getMin() const {
		Vector3f store;
		store.set(center).subtractLocal(xExtent, yExtent, zExtent);
		return store;
	}

	Vector3f getMax() const {
		Vector3f store;
		store.set(center).addLocal(xExtent, yExtent, zExtent);
		return store;
	}

	Vector3f getExtent() const { return Vector3f(xExtent, yExtent, zExtent); }
};

/**
 * Builds the tree of construct() on a copy of the mesh's triangles (their three vertices, read once with Mesh::getTriangle), with the algorithm
 * and the float expressions of createNode/createBox/sortTriangles/setMinMax/getMinMax below: the same nodes, planes and triangle order, without
 * the temporary RefCounted boxes (3.1 million per M4 load) and without the checked Field accesses of a Mesh::getTriangle call per visit. The
 * triangle order is written back to the mesh at the end with the mesh's own swaps.
 */
class TreeBuilder {
public:
	explicit TreeBuilder(scene::Mesh& mesh) : corners(static_cast<size_t>(mesh.getTriangleCount()) * 3), order(corners.size() / 3) {
		for (size_t i = 0; i < order.size(); ++i) {
			order[i] = static_cast<int32_t>(i);
			mesh.getTriangle(static_cast<int32_t>(i), corners[i * 3], corners[i * 3 + 1], corners[i * 3 + 2]);
		}
	}

	runtime::Ref<BIHNode> createNode(int32_t l, int32_t r, const BoxValue& nodeBbox, bool nodeIsMeshBound, int32_t depth) {
		if ((r - l) < BIHTree::MAX_TRIS_PER_NODE || depth > BIHTree::MAX_TREE_DEPTH) {
			return BIHNode::create(l, r);
		}

		BoxValue currentBox = nodeIsMeshBound ? nodeBbox : createBox(l, r);

		Vector3f exteriorExt = nodeBbox.getExtent();
		Vector3f interiorExt = currentBox.getExtent();
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

		float split = currentBox.center.get(axis);
		int32_t pivot = sortTriangles(l, r, split, axis);
		if (pivot == l || pivot == r)
			pivot = (r + l) / 2;

		if (pivot < l) {
			BoxValue rbbox = currentBox;
			setMinMax(rbbox, true, axis, split);
			return createNode(l, r, rbbox, false, depth + 1);
		} else if (pivot > r) {
			BoxValue lbbox = currentBox;
			setMinMax(lbbox, false, axis, split);
			return createNode(l, r, lbbox, false, depth + 1);
		} else {
			runtime::Ref<BIHNode> node = BIHNode::create(axis);

			BoxValue lbbox = currentBox;
			setMinMax(lbbox, false, axis, split);
			node->setLeftPlane(createBox(l, std::max(l, pivot - 1)).getMax().get(axis));
			node->setLeftChild(createNode(l, std::max(l, pivot - 1), lbbox, false, depth + 1));

			BoxValue rbbox = currentBox;
			setMinMax(rbbox, true, axis, split);
			node->setRightPlane(createBox(pivot, r).getMin().get(axis));
			node->setRightChild(createNode(pivot, r, rbbox, false, depth + 1));

			return node;
		}
	}

	/** Applies the triangle order of the build to the mesh (triangle `order[k]` of the original mesh ends at position k). */
	void writeOrder(scene::Mesh& mesh) const {
		std::vector<int32_t> positionOf(order.size());
		std::vector<int32_t> atPosition(order.size());
		for (size_t i = 0; i < order.size(); ++i) {
			positionOf[i] = static_cast<int32_t>(i);
			atPosition[i] = static_cast<int32_t>(i);
		}
		for (size_t k = 0; k < order.size(); ++k) {
			int32_t triangle = order[k];
			int32_t position = positionOf[static_cast<size_t>(triangle)];
			if (position == static_cast<int32_t>(k))
				continue;
			mesh.swapTriangles(static_cast<int32_t>(k), position);
			int32_t displaced = atPosition[k];
			atPosition[static_cast<size_t>(position)] = displaced;
			positionOf[static_cast<size_t>(displaced)] = position;
			atPosition[k] = triangle;
			positionOf[static_cast<size_t>(triangle)] = static_cast<int32_t>(k);
		}
	}

private:
	void getTriangle(int32_t index, Vector3f& v1, Vector3f& v2, Vector3f& v3) const {
		size_t base = static_cast<size_t>(index) * 3;
		v1 = corners[base];
		v2 = corners[base + 1];
		v3 = corners[base + 2];
	}

	void swapTriangles(int32_t i1, int32_t i2) {
		size_t p1 = static_cast<size_t>(i1) * 3;
		size_t p2 = static_cast<size_t>(i2) * 3;
		for (size_t k = 0; k < 3; ++k)
			std::swap(corners[p1 + k], corners[p2 + k]);
		std::swap(order[static_cast<size_t>(i1)], order[static_cast<size_t>(i2)]);
	}

	BoxValue createBox(int32_t l, int32_t r) const {
		constexpr float INF = std::numeric_limits<float>::infinity();
		Vector3f min(INF, INF, INF);
		Vector3f max(-INF, -INF, -INF);
		Vector3f v1, v2, v3;
		for (int32_t i = l; i <= r; i++) {
			getTriangle(i, v1, v2, v3);
			BoundingBox::checkMinMax(min, max, v1);
			BoundingBox::checkMinMax(min, max, v2);
			BoundingBox::checkMinMax(min, max, v3);
		}
		BoxValue box;
		box.setMinMax(min, max);
		return box;
	}

	int32_t sortTriangles(int32_t l, int32_t r, float split, int32_t axis) {
		int32_t pivot = l;
		int32_t j = r;
		Vector3f v1, v2, v3;
		while (pivot <= j) {
			getTriangle(pivot, v1, v2, v3);
			v1.addLocal(v2).addLocal(v3).multLocal(math::FastMath::ONE_THIRD);
			if (v1.get(axis) > split) {
				swapTriangles(pivot, j);
				--j;
			} else {
				++pivot;
			}
		}
		pivot = (pivot == l && j < pivot) ? j : pivot;
		return pivot;
	}

	static void setMinMax(BoxValue& bbox, bool doMin, int32_t axis, float value) {
		Vector3f min = bbox.getMin();
		Vector3f max = bbox.getMax();
		if (doMin)
			min.set(axis, value);
		else
			max.set(axis, value);
		bbox.setMinMax(min, max);
	}

	/** the three vertices of every triangle, in the current triangle order */
	std::vector<Vector3f> corners;
	/** order[k]: the original index of the triangle now at position k */
	std::vector<int32_t> order;
};

} // namespace

BIHTree::BIHTree(scene::Mesh& meshValue) : mesh(meshValue) {
}

BIHTree::~BIHTree() = default;

runtime::Ref<BIHTree> BIHTree::create(scene::Mesh& meshValue) {
	return runtime::makeRef<BIHTree>(meshValue);
}

void BIHTree::construct() {
	int32_t numTris = mesh->getTriangleCount();
	runtime::Ptr<BoundingBox> meshBound = runtime::cast<BoundingBox>(mesh->getBound());
	if ((numTris - 1) < MAX_TRIS_PER_NODE) { // a leaf root reads no triangle
		root.set(createNode(0, numTris - 1, *meshBound, 0));
		return;
	}
	// Performance (M4): TreeBuilder runs the algorithm of createNode on a copy of the triangles (every one is read by the root's sort anyway);
	// the member functions below stay the Java-shaped reference of that algorithm
	TreeBuilder builder(*mesh);
	root.set(builder.createNode(0, numTris - 1, BoxValue::of(*meshBound), true, 0));
	builder.writeOrder(*mesh);
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
