#pragma once

#include <string>

#include "aion/gameserver/model/templates/worldraid/MarkerSpot.xml.h"

namespace aion::gameserver::model::templates::worldraid {

/** Java com.aionemu.gameserver.model.templates.worldraid.MarkerSpot. @author Sykra */
class MarkerSpot : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/worldraid/MarkerSpot.xml.inc"
public:
	/** Java: "MarkerSpot[x=..., y=..., z=..., h=...]" with Float.toString */
	std::string toString() const;
};

} // namespace aion::gameserver::model::templates::worldraid
