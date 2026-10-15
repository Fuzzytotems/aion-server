#include "aion/gameserver/model/templates/worldraid/MarkerSpot.h"

#include "aion/gameserver/geoEngine/math/JavaFloat.h"

namespace aion::gameserver::model::templates::worldraid {

std::string MarkerSpot::toString() const {
	using geoEngine::math::JavaFloat;
	return "MarkerSpot[x=" + JavaFloat::toString(x) + ", y=" + JavaFloat::toString(y) + ", z=" + JavaFloat::toString(z) + ", h=" + std::to_string(h) +
	       ']';
}

} // namespace aion::gameserver::model::templates::worldraid
