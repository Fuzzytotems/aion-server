#include "aion/gameserver/model/templates/flypath/FlightPath.h"

namespace aion::gameserver::model::templates::flypath {

FlightPath::FlightPath(Type typeValue, int32_t idValue, int32_t distanceValue) : type(typeValue), id(idValue), distance(distanceValue) {
}

FlightPath::~FlightPath() = default;

runtime::Ref<FlightPath> FlightPath::create(Type typeValue, int32_t idValue, int32_t distanceValue) {
	return runtime::makeRef<FlightPath>(typeValue, idValue, distanceValue);
}

} // namespace aion::gameserver::model::templates::flypath
