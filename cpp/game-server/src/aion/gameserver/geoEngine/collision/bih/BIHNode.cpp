#include "aion/gameserver/geoEngine/collision/bih/BIHNode.h"

#include <array>
#include <limits>
#include <numbers>
#include <utility>
#include <vector>

#include "aion/gameserver/geoEngine/collision/CollisionResult.h"
#include "aion/gameserver/geoEngine/collision/CollisionResults.h"
#include "aion/gameserver/geoEngine/collision/bih/BIHTree.h"
#include "aion/gameserver/geoEngine/math/FastMath.h"
#include "aion/gameserver/geoEngine/math/JavaFloat.h"
#include "aion/gameserver/geoEngine/math/Matrix4f.h"
#include "aion/gameserver/geoEngine/math/Ray.h"
#include "aion/gameserver/geoEngine/math/Vector3f.h"
#include "aion/gameserver/geoEngine/utils/JavaMathFloat.h"
#include "aion/gameserver/geoEngine/utils/TempVars.h"
// keep last: Java float semantics for the expressions of this file
#include "aion/gameserver/geoEngine/math/StrictFp.h"

namespace aion::gameserver::geoEngine::collision::bih {

using math::Vector3f;
using utils::javaMax;
using utils::javaMin;

namespace {

/**
 * C++ only: empties the thread-local traversal stack (TempVars.bihStack) when intersectWhere leaves by an exception. Java clears the stack only on
 * entry, so an onlyFirst stop or an exception leaves the far-node entries in the ThreadLocal list (harmless under GC). In C++ those entries hold
 * Ref<BIHNode>, and runtime-architecture.md L15 allows no Ref in thread_local storage outside a call: intersectWhere clears the stack before
 * each return, and this guard covers the exception path. vector::clear is noexcept.
 */
class BihStackClearGuard {
public:
	explicit BihStackClearGuard(std::vector<BIHNode::BIHStackData>& stackValue) noexcept : stack(stackValue) {}
	~BihStackClearGuard() { stack.clear(); }
	BihStackClearGuard(const BihStackClearGuard&) = delete;
	BihStackClearGuard& operator=(const BihStackClearGuard&) = delete;

private:
	std::vector<BIHNode::BIHStackData>& stack; // confined: the calling thread's own TempVars stack, for the duration of the call
};

} // namespace

BIHNode::BIHStackData::BIHStackData(runtime::Ref<BIHNode> nodeValue, float minValue, float maxValue)
	: node(std::move(nodeValue)), min(minValue), max(maxValue) {
}

BIHNode::BIHNode(int32_t l, int32_t r) : leftIndex(l), rightIndex(r), axis(3) { // axis 3 indicates leaf
}

BIHNode::BIHNode(int32_t axisValue) : leftIndex(0), rightIndex(0), axis(axisValue) {
}

BIHNode::BIHNode() : leftIndex(0), rightIndex(0), axis(0) {
}

BIHNode::~BIHNode() = default;

runtime::Ref<BIHNode> BIHNode::create(int32_t l, int32_t r) {
	return runtime::makeRef<BIHNode>(l, r);
}

runtime::Ref<BIHNode> BIHNode::create(int32_t axisValue) {
	return runtime::makeRef<BIHNode>(axisValue);
}

runtime::Ref<BIHNode> BIHNode::create() {
	return runtime::makeRef<BIHNode>();
}

void BIHNode::setLeftChild(runtime::Ptr<BIHNode> value) {
	left.set(value);
}

void BIHNode::setRightChild(runtime::Ptr<BIHNode> value) {
	right.set(value);
}

int32_t BIHNode::intersectWhere(math::Ray& r, const math::Matrix4f& worldMatrix, BIHTree& tree, float sceneMin, float sceneMax,
	CollisionResults& results) {
	utils::TempVars& vars = utils::TempVars::get();
	utils::TempVars::ReleaseGuard releaseOnException(vars);
	std::vector<BIHStackData>& stack = vars.bihStack;
	stack.clear();
	// destroyed before releaseOnException: the stack is empty again before the instance goes back to the pool
	BihStackClearGuard clearOnException(stack);

	Vector3f& o = vars.vect1.set(r.getOrigin());
	Vector3f& d = vars.vect2.set(r.getDirection());

	math::Matrix4f inv = worldMatrix.invert();

	inv.mult(r.getOrigin(), r.getOrigin());

	// Fixes rotation collision bug
	inv.multNormal(r.getDirection(), r.getDirection());
	// inv.multNormalAcross(r.getDirection(), r.getDirection());

	const std::array<float, 3> origins{r.getOrigin().x, r.getOrigin().y, r.getOrigin().z};

	const std::array<float, 3> invDirections{1.0f / r.getDirection().x, 1.0f / r.getDirection().y, 1.0f / r.getDirection().z};

	r.getDirection().normalizeLocal();

	Vector3f& v1 = vars.vect3;
	Vector3f& v2 = vars.vect4;
	Vector3f& v3 = vars.vect5;
	int32_t cols = 0;

	stack.emplace_back(runtime::Ref<BIHNode>(*this), sceneMin, sceneMax);
	bool stopped = false; // Java: break stackloop
	while (!stopped && !stack.empty()) {
		BIHStackData data = stack.back();
		stack.pop_back();
		runtime::Ptr<BIHNode> node = data.node;
		float tMin = data.min, tMax = data.max;

		if (tMax < tMin)
			continue;

		bool nextStackEntry = false; // Java: continue stackloop
		while (node->axis != 3) { // while node is not a leaf
			int32_t a = node->axis;

			// find the origin and direction value for the given axis
			float origin = origins.at(static_cast<size_t>(a));
			float invDirection = invDirections.at(static_cast<size_t>(a));

			float tNearSplit, tFarSplit;
			runtime::Ptr<BIHNode> nearNode, farNode;

			tNearSplit = (node->leftPlane.get() - origin) * invDirection;
			tFarSplit = (node->rightPlane.get() - origin) * invDirection;
			nearNode = node->left.get();
			farNode = node->right.get();

			if (invDirection < 0) {
				std::swap(tNearSplit, tFarSplit);
				std::swap(nearNode, farNode);
			}

			if (tMin > tNearSplit && tMax < tFarSplit) {
				nextStackEntry = true;
				break;
			}

			if (tMin > tNearSplit) {
				tMin = javaMax(tMin, tFarSplit);
				node = farNode;
			} else if (tMax < tFarSplit) {
				tMax = javaMin(tMax, tNearSplit);
				node = nearNode;
			} else {
				stack.emplace_back(runtime::Ref<BIHNode>(farNode), javaMax(tMin, tFarSplit), tMax);
				tMax = javaMin(tMax, tNearSplit);
				node = nearNode;
			}
		}
		if (nextStackEntry)
			continue;

		// a leaf
		for (int32_t i = node->leftIndex; i <= node->rightIndex; i++) {
			tree.getTriangle(i, v1, v2, v3);

			float t = r.intersects(v1, v2, v3);
			if (!math::JavaFloat::isInfinite(t)) {
				worldMatrix.mult(v1, v1);
				worldMatrix.mult(v2, v2);
				worldMatrix.mult(v3, v3);
				float t_world = math::Ray(o, d).intersects(v1, v2, v3);
				t = t_world;

				Vector3f& tempVarsContactPoint = vars.vect6.set(d).multLocal(t).addLocal(o);
				float worldSpaceDist = o.distance(tempVarsContactPoint);
				// fix invisible walls
				if (worldSpaceDist > r.limit)
					continue;
				if (results.shouldInvalidateSlopingSurface()) {
					// taken from https://www.scratchapixel.com/lessons/3d-basic-rendering/ray-tracing-rendering-a-triangle/geometry-of-a-triangle
					Vector3f& planeNormal = v2.subtractLocal(v1).crossLocal(v3.subtractLocal(v1)).normalizeLocal();
					double elevationAngleRad = planeNormal.angleBetween(Vector3f::UNIT_Z);
					if (elevationAngleRad > math::FastMath::HALF_PI) // convert angle >90-180° to 0-90° range
						elevationAngleRad = std::numbers::pi - elevationAngleRad;
					if (elevationAngleRad > results.getSlopingSurfaceAngleRad())
						tempVarsContactPoint.setZ(std::numeric_limits<float>::quiet_NaN());
				}
				results.addCollision(CollisionResult(Vector3f(tempVarsContactPoint), worldSpaceDist));
				cols++;
				if (results.isOnlyFirst()) {
					stopped = true;
					break;
				}
			}
		}
	}
	// Deviation: Java leaves the remaining entries of an onlyFirst stop in the thread-local stack; no Ref may stay in thread_local storage (L15)
	stack.clear();
	vars.release();
	r.setOrigin(o);
	r.setDirection(d);
	return cols;
}

} // namespace aion::gameserver::geoEngine::collision::bih
