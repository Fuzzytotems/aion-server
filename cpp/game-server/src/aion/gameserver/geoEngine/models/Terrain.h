#pragma once

#include <cstdint>
#include <span>

#include "aion/gameserver/runtime/fields/Array.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/geoEngine/collision/fwd.h"
#include "aion/gameserver/geoEngine/math/fwd.h"
#include "aion/gameserver/geoEngine/models/fwd.h"

namespace aion::gameserver::geoEngine::models {

/**
 * The terrain heightmap and materials of a geo map.
 * <p>
 * S0c declaration header (docs/design/hub-headers.md §3.5): a member type of GeoMap. RefCounted (fieldmap K4), created with create().
 * results is nullable in collide (GeoMap.canSee passes null); collideNearXY's p1or4/p2/p3 are the caller's scratch vectors (Java TempVars),
 * modified in place.
 */
class Terrain : public runtime::RefCounted {
	AION_MAKE_REF_FRIEND
private:
	static constexpr int32_t HEIGHTMAP_UNIT_SIZE = 2;          // distance between points (always 2 m)
	static constexpr int32_t HEIGHTMAP_MAX_Z_EXCLUSIVE = 2048; // valid z values range from 0 (inclusive) to 2048 (exclusive)

	runtime::Field<int32_t> heightmapXSize{};
	runtime::Field<int32_t> heightmapYSize{};
	runtime::Field<runtime::Ref<runtime::Array<int16_t>>> heightmap{};
	runtime::Field<int32_t> materialsXSize{};
	runtime::Field<int32_t> materialsYSize{};
	runtime::Field<runtime::Ref<runtime::Array<int8_t>>> materials{};

protected:
	Terrain();
	~Terrain() override;

public:
	/** Java: new Terrain() */
	static runtime::Ref<Terrain> create();

	void setHeightmap(std::span<const int16_t> heightmap, int32_t heightmapXSize, int32_t heightmapYSize);

	void setMaterials(std::span<const int8_t> materials, int32_t materialsXSize, int32_t materialsYSize);

	bool hasHeightmap();

	bool hasMaterials();

	void collideAtOrigin(math::Ray& r, collision::CollisionResults& results);

	bool collide(math::Ray& ray, float targetX, float targetY, collision::CollisionResults* results);

private:
	/**
	 * Terrain layout (top view): p1 - p4 are terrain points around given x/y; the faces (p1, p2, p3) and (p2, p3, p4) are checked against the
	 * ray and the first found collision point is added to results (if not null).
	 */
	bool collideNearXY(float x, float y, math::Ray& ray, math::Vector3f& p1or4, math::Vector3f& p2, math::Vector3f& p3,
		collision::CollisionResults* results);

	/**
	 * @return z value at the given position according to the game logic (NaN outside the terrain)
	 */
	float getZ(int32_t xIndex, int32_t yIndex);

	/**
	 * @return z value at the given heightmap index
	 */
	float getZ(int32_t index);

public:
	int32_t getTerrainMaterialAt(float x, float y);

private:
	/**
	 * @return True if (targetX, targetY) is left of the line made by (startX, startY) -> (endX, endY)
	 */
	bool isLeft(float startX, float startY, float endX, float endY, float targetX, float targetY);

	float getMaximumZDiff(const math::Vector3f& v1, const math::Vector3f& v2, const math::Vector3f& v3);
};

} // namespace aion::gameserver::geoEngine::models
