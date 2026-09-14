#pragma once

#include "aion/gameserver/model/templates/worldraid/MarkerSpot.xml.h"

namespace aion::gameserver::model::templates::worldraid {

/** Java com.aionemu.gameserver.model.templates.worldraid.MarkerSpot. @author Sykra */
class MarkerSpot : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/worldraid/MarkerSpot.xml.inc"
public:
};

} // namespace aion::gameserver::model::templates::worldraid
