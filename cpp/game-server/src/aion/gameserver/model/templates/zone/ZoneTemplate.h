#pragma once

#include <string>

#include "aion/gameserver/model/templates/zone/ZoneTemplate.xml.h"
#include "aion/gameserver/world/zone/fwd.h"

namespace aion::gameserver::model::templates::zone {

/** Java com.aionemu.gameserver.model.templates.zone.ZoneTemplate. @author ATracer */
class ZoneTemplate : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/zone/ZoneTemplate.xml.inc"
public:
	/** Gets the value of the name property (the interned name, empty before the name attribute was bound; Java null) */
	const std::string& getXmlName() const { return name; }

	/** Gets the value of the name property. @return the interned zone name, nullptr before the name attribute was bound (Java null) */
	const ::aion::gameserver::world::zone::ZoneName* getName() const { return zoneName; }

private:
	/** Java @XmlTransient String name: the name of the interned zone name */
	std::string name;
	/** Java @XmlTransient ZoneName zoneName */
	const ::aion::gameserver::world::zone::ZoneName* zoneName = nullptr;
};

} // namespace aion::gameserver::model::templates::zone
