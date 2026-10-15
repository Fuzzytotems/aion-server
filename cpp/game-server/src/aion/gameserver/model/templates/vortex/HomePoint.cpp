#include "aion/gameserver/model/templates/vortex/HomePoint.h"

#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::model::templates::vortex {

runtime::Ref<::aion::gameserver::world::WorldPosition> HomePoint::getHomePoint() const {
	runtime::Ref<::aion::gameserver::world::WorldPosition> position = ::aion::gameserver::world::WorldPosition::create(map);
	position->setXYZH(x, y, z, h);
	return position;
}

} // namespace aion::gameserver::model::templates::vortex
