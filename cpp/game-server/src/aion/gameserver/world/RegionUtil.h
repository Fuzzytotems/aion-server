#pragma once

#include <cstdint>

#include "aion/gameserver/world/fwd.h"

namespace aion::gameserver::world {

/**
 * Region id arithmetic of the map regions (2D ids for normal maps, 3D ids for Reshanta).
 * <p>
 * C++: a static-only class (Java has only static members). The (int) casts of the float coordinates keep Java semantics (NaN is 0, out of range
 * values saturate). The one-coordinate getters read WorldConfig.WORLD_REGION_SIZE on every call, like Java.
 *
 * @author ATracer
 */
class RegionUtil {
public:
	RegionUtil() = delete;

	static constexpr int32_t X_3D_OFFSET = 1000000;
	static constexpr int32_t Y_3D_OFFSET = 1000;
	static constexpr int32_t X_2D_OFFSET = 1000;

	static int32_t get2DRegionId(int32_t regionSize, float x, float y);

	static int32_t get3DRegionId(int32_t regionSize, float x, float y, float z);

	static int32_t get2dRegionId(float x, float y);

	static int32_t get3dRegionId(float x, float y, float z);

	static int32_t getXFrom2dRegionId(int32_t regionId);

	static int32_t getYFrom2dRegionId(int32_t regionId);

	static int32_t getXFrom3dRegionId(int32_t regionId);

	static int32_t getYFrom3dRegionId(int32_t regionId);

	static int32_t getZFrom3dRegionId(int32_t regionId);
};

} // namespace aion::gameserver::world
