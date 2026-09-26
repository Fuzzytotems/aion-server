#include "aion/gameserver/geoEngine/models/Terrain.h"

#include <cmath>
#include <cstddef>
#include <limits>
#include <string>

#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/geoEngine/collision/CollisionResult.h"
#include "aion/gameserver/geoEngine/collision/CollisionResults.h"
#include "aion/gameserver/geoEngine/math/JavaFloat.h"
#include "aion/gameserver/geoEngine/math/Ray.h"
#include "aion/gameserver/geoEngine/math/Vector3f.h"
#include "aion/gameserver/geoEngine/utils/JavaMathFloat.h"
#include "aion/gameserver/geoEngine/utils/TempVars.h"
// keep last: Java float semantics for the expressions of this file
#include "aion/gameserver/geoEngine/math/StrictFp.h"

namespace aion::gameserver::geoEngine::models {

using math::JavaFloat;
using math::Vector3f;

namespace {

constexpr float NOT_A_NUMBER = std::numeric_limits<float>::quiet_NaN();

/** Java: (int) floatValue (NaN is 0, out of range values saturate) */
int32_t floatToInt(float value) {
	return JavaFloat::doubleToInt(static_cast<double>(value));
}

} // namespace

Terrain::Terrain() = default;

Terrain::~Terrain() = default;

runtime::Ref<Terrain> Terrain::create() {
	return runtime::makeRef<Terrain>();
}

void Terrain::setHeightmap(std::span<const int16_t> value, int32_t heightmapXSizeValue, int32_t heightmapYSizeValue) {
	if (materials.get() != nullptr && (heightmapXSizeValue < materialsXSize.get() || heightmapYSizeValue < materialsYSize.get()))
		throw runtime::IllegalArgumentException("Terrain heightmap must not be smaller than terrain materials");
	int64_t lengthDiff = static_cast<int64_t>(value.size()) - static_cast<int32_t>(static_cast<uint32_t>(heightmapXSizeValue) *
		static_cast<uint32_t>(heightmapYSizeValue)); // Java int multiplication
	if (lengthDiff != 0)
		throw runtime::IllegalArgumentException("Expected terrain heightmap length differs by " + std::to_string(lengthDiff) + " bytes");
	bool allSameZValues = !value.empty();
	for (int16_t z : value) {
		if (z != value[0]) {
			allSameZValues = false;
			break;
		}
	}
	runtime::Ref<runtime::Array<int16_t>> array = runtime::Array<int16_t>::make(allSameZValues ? 1 : static_cast<int32_t>(value.size()));
	runtime::Array<int16_t>& elements = *array; // one checked dereference, not one per element (performance)
	for (int32_t i = 0; i < elements.length(); ++i)
		elements[i].set(value[static_cast<size_t>(i)]);
	heightmap.set(std::move(array));
	heightmapXSize.set(heightmapXSizeValue);
	heightmapYSize.set(heightmapYSizeValue);
}

void Terrain::setMaterials(std::span<const int8_t> value, int32_t materialsXSizeValue, int32_t materialsYSizeValue) {
	if (heightmap.get() != nullptr && (materialsXSizeValue > heightmapXSize.get() || materialsYSizeValue > heightmapYSize.get()))
		throw runtime::IllegalArgumentException("Terrain materials need a terrain heightmap of at least the same size");
	int64_t lengthDiff = static_cast<int64_t>(value.size()) - static_cast<int32_t>(static_cast<uint32_t>(materialsXSizeValue) *
		static_cast<uint32_t>(materialsYSizeValue)); // Java int multiplication
	if (lengthDiff != 0)
		throw runtime::IllegalArgumentException("Expected terrain materials length differs by " + std::to_string(lengthDiff) + " bytes");
	runtime::Ref<runtime::Array<int8_t>> array = runtime::Array<int8_t>::make(static_cast<int32_t>(value.size()));
	runtime::Array<int8_t>& elements = *array; // one checked dereference, not one per element (performance)
	for (size_t i = 0; i < value.size(); ++i)
		elements[static_cast<int32_t>(i)].set(value[i]);
	materials.set(std::move(array));
	materialsXSize.set(materialsXSizeValue);
	materialsYSize.set(materialsYSizeValue);
}

bool Terrain::hasHeightmap() {
	return heightmap.get() != nullptr;
}

bool Terrain::hasMaterials() {
	return materials.get() != nullptr;
}

void Terrain::collideAtOrigin(math::Ray& r, collision::CollisionResults& results) {
	utils::TempVars& vars = utils::TempVars::get();
	utils::TempVars::ReleaseGuard releaseOnException(vars);
	collideNearXY(r.origin.x, r.origin.y, r, vars.vect1, vars.vect2, vars.vect3, &results);
	vars.release();
}

bool Terrain::collide(math::Ray& ray, float targetX, float targetY, collision::CollisionResults* results) {
	float distanceX = targetX - ray.origin.x;
	float distanceY = targetY - ray.origin.y;
	float distance2D = static_cast<float>(std::sqrt(static_cast<double>(distanceX * distanceX + distanceY * distanceY)));
	float checkDistanceLimit = distance2D + HEIGHTMAP_UNIT_SIZE;
	utils::TempVars& vars = utils::TempVars::get();
	utils::TempVars::ReleaseGuard releaseOnException(vars);
	for (int32_t checkDistance = 0; static_cast<float>(checkDistance) < checkDistanceLimit; checkDistance += HEIGHTMAP_UNIT_SIZE) {
		float distanceFactor = static_cast<float>(checkDistance) / distance2D;
		float x = ray.origin.x + distanceX * distanceFactor;
		float y = ray.origin.y + distanceY * distanceFactor;
		if (collideNearXY(x, y, ray, vars.vect1, vars.vect2, vars.vect3, results) ||
			collideNearXY(x + HEIGHTMAP_UNIT_SIZE, y, ray, vars.vect1, vars.vect2, vars.vect3, results) ||
			collideNearXY(x, y + HEIGHTMAP_UNIT_SIZE, ray, vars.vect1, vars.vect2, vars.vect3, results)) {
			vars.release();
			return true;
		}
	}
	vars.release();
	return false;
}

bool Terrain::collideNearXY(float x, float y, math::Ray& ray, Vector3f& p1or4, Vector3f& p2, Vector3f& p3, collision::CollisionResults* results) {
	int32_t xIndexNorth = floatToInt(x / HEIGHTMAP_UNIT_SIZE);
	int32_t yIndexWest = floatToInt(y / HEIGHTMAP_UNIT_SIZE);
	int32_t yIndexEast = yIndexWest + 1;
	float z2 = getZ(xIndexNorth, yIndexEast);
	if (JavaFloat::isNaN(z2))
		return false;
	int32_t xIndexSouth = xIndexNorth + 1;
	float z3 = getZ(xIndexSouth, yIndexWest);
	if (JavaFloat::isNaN(z3))
		return false;
	float z1 = getZ(xIndexNorth, yIndexWest);
	float z4 = getZ(xIndexSouth, yIndexEast);
	int32_t xNorth = xIndexNorth * HEIGHTMAP_UNIT_SIZE;
	int32_t yWest = yIndexWest * HEIGHTMAP_UNIT_SIZE;
	int32_t yEast = yWest + HEIGHTMAP_UNIT_SIZE;
	int32_t xSouth = xNorth + HEIGHTMAP_UNIT_SIZE;
	p2.set(static_cast<float>(xNorth), static_cast<float>(yEast), z2);
	p3.set(static_cast<float>(xSouth), static_cast<float>(yWest), z3);
	Vector3f contactPoint;
	if ((JavaFloat::isNaN(z1) || !ray.intersectWhere(p1or4.set(static_cast<float>(xNorth), static_cast<float>(yWest), z1), p2, p3, contactPoint)) &&
		(JavaFloat::isNaN(z4) || !ray.intersectWhere(p1or4.set(static_cast<float>(xSouth), static_cast<float>(yEast), z4), p2, p3, contactPoint)))
		return false;
	float distance = contactPoint.distance(ray.origin);
	if (distance > ray.getLimit())
		return false;
	if (results != nullptr) {
		if (results->shouldInvalidateSlopingSurface() && getMaximumZDiff(p1or4, p2, p3) > HEIGHTMAP_UNIT_SIZE) // height diff >2m means >45° elevation
			contactPoint.setZ(NOT_A_NUMBER);
		results->addCollision(collision::CollisionResult(contactPoint, distance));
	}
	return true;
}

float Terrain::getZ(int32_t xIndex, int32_t yIndex) {
	const int32_t xSize = heightmapXSize.get();
	const int32_t ySize = heightmapYSize.get();
	if (xIndex < 0 || yIndex < 0 || xIndex > xSize || yIndex > ySize)
		return NOT_A_NUMBER;
	if (xIndex == 0 || yIndex == 0 || xIndex == xSize || yIndex == ySize)
		return 0;
	if (heightmap->length() == 1) // simple flat terrain (memory optimized)
		return getZ(0);
	return getZ(yIndex + (xIndex * ySize));
}

float Terrain::getZ(int32_t index) {
	int16_t z = heightmap->get(index);
	return z == -1 ? NOT_A_NUMBER : static_cast<float>(static_cast<uint16_t>(z) * HEIGHTMAP_MAX_Z_EXCLUSIVE) / (0xFFFF + 1.0f);
}

int32_t Terrain::getTerrainMaterialAt(float x, float y) {
	runtime::Ptr<runtime::Array<int8_t>> mats = materials.get();
	if (mats == nullptr)
		return 0;
	const int32_t xSize = materialsXSize.get();
	const int32_t ySize = materialsYSize.get();
	int32_t mat1x = floatToInt(x / HEIGHTMAP_UNIT_SIZE);
	int32_t mat1y = floatToInt(y / HEIGHTMAP_UNIT_SIZE);
	if (mat1x < 0 || mat1y < 0 || mat1x >= xSize || mat1y >= ySize)
		return 0;
	int32_t mat1Index = mat1y + (mat1x * ySize);
	int32_t mat3Index = mat1Index + ySize;
	int32_t mat = mats->get(mat1Index);
	// check whether triangle points p1, p2, p3 have materials assigned
	if (mat != 0 && mat == mats->get(mat1Index + 1) && mat == mats->get(mat3Index)) {
		if (isLeft(x + HEIGHTMAP_UNIT_SIZE, y, x, y + HEIGHTMAP_UNIT_SIZE, x, y)) { // check if x, y is in triangle
			return static_cast<uint8_t>(mats->get(mat1Index));
		}
	}
	if ((mat3Index + 1) < mats->length() && (mat = mats->get(mat3Index + 1)) != 0 && mat == mats->get(mat3Index) &&
		mat == mats->get(mat1Index + 1)) { // check whether triangle points p2, p3, p4 have materials assigned
		if (!isLeft(x + HEIGHTMAP_UNIT_SIZE, y, x, y + HEIGHTMAP_UNIT_SIZE, x, y)) { // check if x, y is in triangle
			return static_cast<uint8_t>(mats->get(mat3Index + 1));
		}
	}
	return 0;
}

bool Terrain::isLeft(float startX, float startY, float endX, float endY, float targetX, float targetY) {
	return (endX - startX) * (targetY - startY) > (endY - startY) * (targetX - startX);
}

float Terrain::getMaximumZDiff(const Vector3f& v1, const Vector3f& v2, const Vector3f& v3) {
	return utils::javaMax(v1.z, utils::javaMax(v2.z, v3.z)) - utils::javaMin(v1.z, utils::javaMin(v2.z, v3.z));
}

} // namespace aion::gameserver::geoEngine::models
