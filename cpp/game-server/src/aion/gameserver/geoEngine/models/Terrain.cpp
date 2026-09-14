#include "aion/gameserver/geoEngine/models/Terrain.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/geoEngine/collision/CollisionResults.h"

namespace aion::gameserver::geoEngine::models {

Terrain::Terrain() = default;

Terrain::~Terrain() = default;

runtime::Ref<Terrain> Terrain::create() {
	return runtime::makeRef<Terrain>();
}

void Terrain::setHeightmap(std::span<const int16_t> value, int32_t heightmapXSizeValue, int32_t heightmapYSizeValue) {
	AION_UNPORTED();
}

void Terrain::setMaterials(std::span<const int8_t> value, int32_t materialsXSizeValue, int32_t materialsYSizeValue) {
	AION_UNPORTED();
}

bool Terrain::hasHeightmap() {
	AION_UNPORTED();
}

bool Terrain::hasMaterials() {
	AION_UNPORTED();
}

void Terrain::collideAtOrigin(math::Ray& r, collision::CollisionResults& results) {
	AION_UNPORTED();
}

bool Terrain::collide(math::Ray& ray, float targetX, float targetY, collision::CollisionResults* results) {
	AION_UNPORTED();
}

bool Terrain::collideNearXY(float x, float y, math::Ray& ray, math::Vector3f& p1or4, math::Vector3f& p2, math::Vector3f& p3,
	collision::CollisionResults* results) {
	AION_UNPORTED();
}

float Terrain::getZ(int32_t xIndex, int32_t yIndex) {
	AION_UNPORTED();
}

float Terrain::getZ(int32_t index) {
	AION_UNPORTED();
}

int32_t Terrain::getTerrainMaterialAt(float x, float y) {
	AION_UNPORTED();
}

bool Terrain::isLeft(float startX, float startY, float endX, float endY, float targetX, float targetY) {
	AION_UNPORTED();
}

float Terrain::getMaximumZDiff(const math::Vector3f& v1, const math::Vector3f& v2, const math::Vector3f& v3) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::geoEngine::models
