#include "aion/gameserver/world/RegionUtil.h"

#include "aion/gameserver/configs/main/WorldConfig.h"
#include "aion/gameserver/geoEngine/math/JavaFloat.h"

namespace aion::gameserver::world {

namespace {

using geoEngine::math::JavaFloat;

/** Java: (int) x / regionSize (integer division truncating toward zero, like Java) */
int32_t cell(float coordinate, int32_t regionSize) {
	return JavaFloat::doubleToInt(coordinate) / regionSize;
}

int32_t regionSizeConfig() {
	return configs::main::WorldConfig::WORLD_REGION_SIZE.load();
}

} // namespace

int32_t RegionUtil::get2DRegionId(int32_t regionSize, float x, float y) {
	return cell(x, regionSize) * X_2D_OFFSET + cell(y, regionSize);
}

int32_t RegionUtil::get3DRegionId(int32_t regionSize, float x, float y, float z) {
	return cell(x, regionSize) * X_3D_OFFSET + cell(y, regionSize) * Y_3D_OFFSET + cell(z, regionSize);
}

int32_t RegionUtil::get2dRegionId(float x, float y) {
	return get2DRegionId(regionSizeConfig(), x, y);
}

int32_t RegionUtil::get3dRegionId(float x, float y, float z) {
	return get3DRegionId(regionSizeConfig(), x, y, z);
}

int32_t RegionUtil::getXFrom2dRegionId(int32_t regionId) {
	return regionId / X_2D_OFFSET * regionSizeConfig();
}

int32_t RegionUtil::getYFrom2dRegionId(int32_t regionId) {
	return regionId % X_2D_OFFSET * regionSizeConfig();
}

int32_t RegionUtil::getXFrom3dRegionId(int32_t regionId) {
	return regionId / X_3D_OFFSET * regionSizeConfig();
}

int32_t RegionUtil::getYFrom3dRegionId(int32_t regionId) {
	return regionId % X_3D_OFFSET / Y_3D_OFFSET * regionSizeConfig();
}

int32_t RegionUtil::getZFrom3dRegionId(int32_t regionId) {
	return regionId % X_3D_OFFSET % Y_3D_OFFSET * regionSizeConfig();
}

} // namespace aion::gameserver::world
