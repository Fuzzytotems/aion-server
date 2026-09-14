#pragma once

#include "aion/gameserver/model/templates/siegelocation/SiegeLocationTemplate.xml.h"

namespace aion::gameserver::model::templates::siegelocation {

/** Java com.aionemu.gameserver.model.templates.siegelocation.SiegeLocationTemplate. @author Sarynth, antness, Source, Wakizashi */
class SiegeLocationTemplate : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/siegelocation/SiegeLocationTemplate.xml.inc"
public:
};

} // namespace aion::gameserver::model::templates::siegelocation
