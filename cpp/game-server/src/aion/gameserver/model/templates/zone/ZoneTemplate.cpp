#include "aion/gameserver/model/templates/zone/ZoneTemplate.h"

#include "aion/gameserver/world/zone/ZoneName.h"

namespace aion::gameserver::model::templates::zone {

void ZoneTemplate::setXmlName(std::string_view value) {
	zoneName = ::aion::gameserver::world::zone::ZoneName::createOrGet(value);
	name = zoneName->name();
}

} // namespace aion::gameserver::model::templates::zone
