#include "aion/gameserver/geoEngine/bounding/BoundingBox.h"

#include <limits>

#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/geoEngine/collision/CollisionResult.h"
#include "aion/gameserver/geoEngine/collision/CollisionResults.h"
#include "aion/gameserver/geoEngine/math/FastMath.h"
#include "aion/gameserver/geoEngine/math/JavaFloat.h"
#include "aion/gameserver/geoEngine/math/Matrix3f.h"
#include "aion/gameserver/geoEngine/math/Matrix4f.h"
#include "aion/gameserver/geoEngine/math/Ray.h"
#include "aion/gameserver/geoEngine/utils/TempVars.h"
// keep last: Java float semantics for the expressions of this file
#include "aion/gameserver/geoEngine/math/StrictFp.h"

namespace aion::gameserver::geoEngine::bounding {

using math::FastMath;
using math::Vector3f;
using utils::TempVars;

namespace {
constexpr float POSITIVE_INFINITY = std::numeric_limits<float>::infinity();
constexpr float NEGATIVE_INFINITY = -std::numeric_limits<float>::infinity();
} // namespace

BoundingBox::BoundingBox() = default;

BoundingBox::BoundingBox(const Vector3f& c, float x, float y, float z) {
	center.set(c);
	xExtent.set(x);
	yExtent.set(y);
	zExtent.set(z);
}

BoundingBox::BoundingBox(BoundingBox& source) {
	center.set(source.center.get());
	xExtent.set(source.xExtent.get());
	yExtent.set(source.yExtent.get());
	zExtent.set(source.zExtent.get());
}

BoundingBox::BoundingBox(const Vector3f& min, const Vector3f& max) {
	setMinMax(min, max);
}

BoundingBox::~BoundingBox() = default;

runtime::Ref<BoundingBox> BoundingBox::create() {
	return runtime::makeRef<BoundingBox>();
}

runtime::Ref<BoundingBox> BoundingBox::create(const Vector3f& c, float x, float y, float z) {
	return runtime::makeRef<BoundingBox>(c, x, y, z);
}

runtime::Ref<BoundingBox> BoundingBox::create(BoundingBox& source) {
	return runtime::makeRef<BoundingBox>(source);
}

runtime::Ref<BoundingBox> BoundingBox::create(const Vector3f& min, const Vector3f& max) {
	return runtime::makeRef<BoundingBox>(min, max);
}

BoundingVolume::Type BoundingBox::getType() {
	return Type::AABB;
}

void BoundingBox::computeFromPoints(std::span<const float> points) {
	containAABB(points);
}

void BoundingBox::checkMinMax(Vector3f& min, Vector3f& max, const Vector3f& point) {
	if (point.x < min.x)
		min.x = point.x;
	if (point.x > max.x)
		max.x = point.x;
	if (point.y < min.y)
		min.y = point.y;
	if (point.y > max.y)
		max.y = point.y;
	if (point.z < min.z)
		min.z = point.z;
	if (point.z > max.z)
		max.z = point.z;
}

void BoundingBox::containAABB(std::span<const float> points) {
	if (points.size() <= 2) // we need at least a 3 float vector
		throw runtime::IllegalArgumentException("");

	float minX = POSITIVE_INFINITY, minY = POSITIVE_INFINITY, minZ = POSITIVE_INFINITY;
	float maxX = NEGATIVE_INFINITY, maxY = NEGATIVE_INFINITY, maxZ = NEGATIVE_INFINITY;

	TempVars& vars = TempVars::get();
	TempVars::ReleaseGuard releaseOnException(vars);
	Vector3f& vect1 = vars.vect1;
	for (size_t i = 0; i < points.size();) {
		// Java: points.get(i++) throws IndexOutOfBoundsException for a length that is not a multiple of 3
		if (i + 2 >= points.size())
			throw runtime::IndexOutOfBoundsException("");
		vect1.x = points[i++];
		vect1.y = points[i++];
		vect1.z = points[i++];
		if (vect1.x < minX)
			minX = vect1.x;
		if (vect1.x > maxX)
			maxX = vect1.x;

		if (vect1.y < minY)
			minY = vect1.y;
		if (vect1.y > maxY)
			maxY = vect1.y;

		if (vect1.z < minZ)
			minZ = vect1.z;
		if (vect1.z > maxZ)
			maxZ = vect1.z;
	}
	vars.release();

	// Deviation: Java stores minX + maxX into center and halves it afterwards, so a thread reading a mesh bound that another loader thread is
	// recomputing (the maps load in parallel and share meshes) can see the doubled center; the final center is stored in one step here.
	Vector3f c(minX + maxX, minY + maxY, minZ + maxZ);
	c.multLocal(0.5f);
	center.set(c);

	xExtent.set(maxX - c.x);
	yExtent.set(maxY - c.y);
	zExtent.set(maxZ - c.z);
}

runtime::Ref<BoundingVolume> BoundingBox::transform(const math::Matrix4f& trans, runtime::Ptr<BoundingVolume> store) {
	runtime::Ref<BoundingBox> box;
	if (store == nullptr || store->getType() != Type::AABB) {
		box = BoundingBox::create();
	} else {
		box = runtime::cast<BoundingBox>(store);
	}

	Vector3f boxCenter = box->center.get();
	float w = trans.multProj(center.get(), boxCenter);
	boxCenter.divideLocal(w);
	box->center.set(boxCenter);

	TempVars& vars = TempVars::get();
	TempVars::ReleaseGuard releaseOnException(vars);
	math::Matrix3f& transMatrix = vars.tempMat3;
	trans.toRotationMatrix(transMatrix);

	// Make the rotation matrix all positive to get the maximum x/y/z extent
	transMatrix.absoluteLocal();

	vars.vect1.set(xExtent.get(), yExtent.get(), zExtent.get());
	transMatrix.mult(vars.vect1, vars.vect1);

	// Assign the biggest rotations after scales.
	box->xExtent.set(FastMath::abs(vars.vect1.getX()));
	box->yExtent.set(FastMath::abs(vars.vect1.getY()));
	box->zExtent.set(FastMath::abs(vars.vect1.getZ()));

	vars.release();
	return box;
}

runtime::Ptr<BoundingVolume> BoundingBox::mergeLocal(runtime::Ptr<BoundingVolume> volume) {
	if (volume == nullptr) {
		return runtime::Ptr<BoundingVolume>(*this);
	}

	switch (volume->getType()) {
		case Type::AABB: {
			runtime::Ptr<BoundingBox> vBox = runtime::cast<BoundingBox>(volume);
			return runtime::Ptr<BoundingVolume>(mergeLocal(vBox->center.get(), vBox->xExtent.get(), vBox->yExtent.get(), vBox->zExtent.get()));
		}

		// case OBB: {
		// return mergeOBB((OrientedBoundingBox) volume);
		// }

		default:
			return nullptr;
	}
}

BoundingBox& BoundingBox::mergeLocal(const Vector3f& boxCenter, float boxX, float boxY, float boxZ) {
	Vector3f c = center.get();
	if (xExtent.get() == POSITIVE_INFINITY || boxX == POSITIVE_INFINITY) {
		c.x = 0;
		center.set(c);
		xExtent.set(POSITIVE_INFINITY);
	} else {
		float low = c.x - xExtent.get();
		if (low > boxCenter.x - boxX) {
			low = boxCenter.x - boxX;
		}
		float high = c.x + xExtent.get();
		if (high < boxCenter.x + boxX) {
			high = boxCenter.x + boxX;
		}
		c.x = (low + high) / 2;
		center.set(c);
		xExtent.set(high - c.x);
	}

	c = center.get();
	if (yExtent.get() == POSITIVE_INFINITY || boxY == POSITIVE_INFINITY) {
		c.y = 0;
		center.set(c);
		yExtent.set(POSITIVE_INFINITY);
	} else {
		float low = c.y - yExtent.get();
		if (low > boxCenter.y - boxY) {
			low = boxCenter.y - boxY;
		}
		float high = c.y + yExtent.get();
		if (high < boxCenter.y + boxY) {
			high = boxCenter.y + boxY;
		}
		c.y = (low + high) / 2;
		center.set(c);
		yExtent.set(high - c.y);
	}

	c = center.get();
	if (zExtent.get() == POSITIVE_INFINITY || boxZ == POSITIVE_INFINITY) {
		c.z = 0;
		center.set(c);
		zExtent.set(POSITIVE_INFINITY);
	} else {
		float low = c.z - zExtent.get();
		if (low > boxCenter.z - boxZ) {
			low = boxCenter.z - boxZ;
		}
		float high = c.z + zExtent.get();
		if (high < boxCenter.z + boxZ) {
			high = boxCenter.z + boxZ;
		}
		c.z = (low + high) / 2;
		center.set(c);
		zExtent.set(high - c.z);
	}

	return *this;
}

runtime::Ref<BoundingVolume> BoundingBox::clone(runtime::Ptr<BoundingVolume> store) {
	runtime::Ref<BoundingBox> rVal;
	if (store != nullptr && store->getType() == Type::AABB)
		rVal = runtime::cast<BoundingBox>(store);
	else
		rVal = BoundingBox::create();
	rVal->center.set(center.get());
	rVal->xExtent.set(xExtent.get());
	rVal->yExtent.set(yExtent.get());
	rVal->zExtent.set(zExtent.get());
	return rVal;
}

std::string BoundingBox::toString() {
	return "BoundingBox [Center: " + center.get().toString() + "  xExtent: " + math::JavaFloat::toString(xExtent.get()) +
		   "  yExtent: " + math::JavaFloat::toString(yExtent.get()) + "  zExtent: " + math::JavaFloat::toString(zExtent.get()) + "]";
}

bool BoundingBox::intersects(BoundingVolume& bv) {
	return bv.intersectsBoundingBox(*this);
}

bool BoundingBox::intersectsBoundingBox(BoundingBox& bb) {
	const Vector3f c = center.get();
	const Vector3f bbCenter = bb.center.get();
	const float xe = xExtent.get(), ye = yExtent.get(), ze = zExtent.get();
	const float bbXe = bb.xExtent.get(), bbYe = bb.yExtent.get(), bbZe = bb.zExtent.get();
	if (c.x + xe < bbCenter.x - bbXe || c.x - xe > bbCenter.x + bbXe)
		return false;
	else if (c.y + ye < bbCenter.y - bbYe || c.y - ye > bbCenter.y + bbYe)
		return false;
	else if (c.z + ze < bbCenter.z - bbZe || c.z - ze > bbCenter.z + bbZe)
		return false;
	else
		return true;
}

bool BoundingBox::intersects(const math::Ray& ray) {
	TempVars& vars = TempVars::get();
	TempVars::ReleaseGuard releaseOnException(vars);
	Vector3f diff = ray.origin.subtract(getCenter(vars.vect2), vars.vect1);

	std::array<float, 3>& fWdU = vars.fWdU;
	std::array<float, 3>& fAWdU = vars.fAWdU;
	std::array<float, 3>& fDdU = vars.fDdU;
	std::array<float, 3>& fADdU = vars.fADdU;
	std::array<float, 3>& fAWxDdU = vars.fAWxDdU;

	const float xe = xExtent.get(), ye = yExtent.get(), ze = zExtent.get();

	fWdU[0] = ray.getDirection().dot(Vector3f::UNIT_X);
	fAWdU[0] = FastMath::abs(fWdU[0]);
	fDdU[0] = diff.dot(Vector3f::UNIT_X);
	fADdU[0] = FastMath::abs(fDdU[0]);
	if (fADdU[0] > xe && fDdU[0] * fWdU[0] >= 0.0) {
		vars.release();
		return false;
	}

	fWdU[1] = ray.getDirection().dot(Vector3f::UNIT_Y);
	fAWdU[1] = FastMath::abs(fWdU[1]);
	fDdU[1] = diff.dot(Vector3f::UNIT_Y);
	fADdU[1] = FastMath::abs(fDdU[1]);
	if (fADdU[1] > ye && fDdU[1] * fWdU[1] >= 0.0) {
		vars.release();
		return false;
	}

	fWdU[2] = ray.getDirection().dot(Vector3f::UNIT_Z);
	fAWdU[2] = FastMath::abs(fWdU[2]);
	fDdU[2] = diff.dot(Vector3f::UNIT_Z);
	fADdU[2] = FastMath::abs(fDdU[2]);
	if (fADdU[2] > ze && fDdU[2] * fWdU[2] >= 0.0) {
		vars.release();
		return false;
	}

	Vector3f wCrossD = ray.getDirection().cross(diff, vars.vect2);

	fAWxDdU[0] = FastMath::abs(wCrossD.dot(Vector3f::UNIT_X));
	float rhs = ye * fAWdU[2] + ze * fAWdU[1];
	if (fAWxDdU[0] > rhs) {
		vars.release();
		return false;
	}

	fAWxDdU[1] = FastMath::abs(wCrossD.dot(Vector3f::UNIT_Y));
	rhs = xe * fAWdU[2] + ze * fAWdU[0];
	if (fAWxDdU[1] > rhs) {
		vars.release();
		return false;
	}

	fAWxDdU[2] = FastMath::abs(wCrossD.dot(Vector3f::UNIT_Z));
	rhs = xe * fAWdU[1] + ye * fAWdU[0];
	if (fAWxDdU[2] > rhs) {
		vars.release();
		return false;
	}

	vars.release();
	return true;
}

int32_t BoundingBox::collideWithRay(const math::Ray& ray, collision::CollisionResults& results) {
	TempVars& vars = TempVars::get();
	TempVars::ReleaseGuard releaseOnException(vars);
	Vector3f& diff = vars.vect1.set(ray.origin).subtractLocal(center.get());
	Vector3f& direction = vars.vect2.set(ray.direction);

	std::array<float, 3>& t = vars.fWdU; // use one of the TempVars arrays
	t[0] = 0;
	t[1] = ray.getLimit();
	int32_t collisions = 0;

	const float xe = xExtent.get(), ye = yExtent.get(), ze = zExtent.get();
	float saveT0 = t[0], saveT1 = t[1];
	bool notEntirelyClipped = clip(+direction.x, -diff.x - xe, t) && clip(-direction.x, +diff.x - xe, t) && clip(+direction.y, -diff.y - ye, t) &&
							  clip(-direction.y, +diff.y - ye, t) && clip(+direction.z, -diff.z - ze, t) && clip(-direction.z, +diff.z - ze, t);

	if (notEntirelyClipped && (t[0] != saveT0 || t[1] != saveT1)) {
		Vector3f contactPoint1 = Vector3f(ray.direction).multLocal(t[0]).addLocal(ray.origin);
		results.addCollision(collision::CollisionResult(contactPoint1, t[0]));
		collisions++;
		if (t[1] > t[0]) {
			Vector3f contactPoint2 = Vector3f(ray.direction).multLocal(t[1]).addLocal(ray.origin);
			results.addCollision(collision::CollisionResult(contactPoint2, t[1]));
			collisions++;
		}
	}
	vars.release();
	return collisions;
}

int32_t BoundingBox::collideWith(math::Ray& other, collision::CollisionResults& results) {
	return collideWithRay(other, results);
}

bool BoundingBox::contains(const Vector3f& point) {
	const Vector3f c = center.get();
	return FastMath::abs(c.x - point.x) < xExtent.get() && FastMath::abs(c.y - point.y) < yExtent.get() && FastMath::abs(c.z - point.z) < zExtent.get();
}

bool BoundingBox::intersects(const Vector3f& point) {
	const Vector3f c = center.get();
	return FastMath::abs(c.x - point.x) <= xExtent.get() && FastMath::abs(c.y - point.y) <= yExtent.get() &&
		   FastMath::abs(c.z - point.z) <= zExtent.get();
}

float BoundingBox::distanceToEdge(const Vector3f& point) {
	// compute coordinates of point in box coordinate system
	TempVars& vars = TempVars::get();
	TempVars::ReleaseGuard releaseOnException(vars);
	Vector3f& closest = point.subtract(center.get(), vars.vect1);

	const float xe = xExtent.get(), ye = yExtent.get(), ze = zExtent.get();
	// project test point onto box
	float sqrDistance = 0.0f;
	float delta;

	if (closest.x < -xe) {
		delta = closest.x + xe;
		sqrDistance += delta * delta;
	} else if (closest.x > xe) {
		delta = closest.x - xe;
		sqrDistance += delta * delta;
	}

	if (closest.y < -ye) {
		delta = closest.y + ye;
		sqrDistance += delta * delta;
	} else if (closest.y > ye) {
		delta = closest.y - ye;
		sqrDistance += delta * delta;
	}

	if (closest.z < -ze) {
		delta = closest.z + ze;
		sqrDistance += delta * delta;
	} else if (closest.z > ze) {
		delta = closest.z - ze;
		sqrDistance += delta * delta;
	}

	vars.release();
	return FastMath::sqrt(sqrDistance);
}

bool BoundingBox::clip(float denom, float numer, std::span<float, 3> t) {
	// Return value is 'true' if line segment intersects the current test plane. Otherwise 'false' is returned in which case the line segment
	// is entirely clipped.
	if (denom > 0.0f) {
		if (numer > denom * t[1])
			return false;
		if (numer > denom * t[0])
			t[0] = numer / denom;
		return true;
	} else if (denom < 0.0f) {
		if (numer > denom * t[0])
			return false;
		if (numer > denom * t[1])
			t[1] = numer / denom;
		return true;
	} else {
		return numer <= 0.0;
	}
}

Vector3f BoundingBox::getExtent() {
	Vector3f store;
	return getExtent(store);
}

Vector3f& BoundingBox::getExtent(Vector3f& store) {
	store.set(xExtent.get(), yExtent.get(), zExtent.get());
	return store;
}

void BoundingBox::setXExtent(float value) {
	if (value < 0)
		throw runtime::IllegalArgumentException("");
	xExtent.set(value);
}

void BoundingBox::setYExtent(float value) {
	if (value < 0)
		throw runtime::IllegalArgumentException("");
	yExtent.set(value);
}

void BoundingBox::setZExtent(float value) {
	if (value < 0)
		throw runtime::IllegalArgumentException("");
	zExtent.set(value);
}

Vector3f BoundingBox::getMin() {
	Vector3f store;
	return getMin(store);
}

Vector3f& BoundingBox::getMin(Vector3f& store) {
	store.set(center.get()).subtractLocal(xExtent.get(), yExtent.get(), zExtent.get());
	return store;
}

Vector3f BoundingBox::getMax() {
	Vector3f store;
	return getMax(store);
}

Vector3f& BoundingBox::getMax(Vector3f& store) {
	store.set(center.get()).addLocal(xExtent.get(), yExtent.get(), zExtent.get());
	return store;
}

void BoundingBox::setMinMax(const Vector3f& min, const Vector3f& max) {
	Vector3f c = Vector3f(max).addLocal(min).multLocal(0.5f);
	center.set(c);
	xExtent.set(FastMath::abs(max.x - c.x));
	yExtent.set(FastMath::abs(max.y - c.y));
	zExtent.set(FastMath::abs(max.z - c.z));
}

float BoundingBox::getVolume() {
	return (8 * xExtent.get() * yExtent.get() * zExtent.get());
}

} // namespace aion::gameserver::geoEngine::bounding
