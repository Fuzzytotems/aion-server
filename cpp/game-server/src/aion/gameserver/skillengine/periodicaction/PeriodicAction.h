#pragma once

#include "aion/gameserver/skillengine/periodicaction/PeriodicAction.xml.h"

namespace aion::gameserver::skillengine::periodicaction {

/** Java com.aionemu.gameserver.skillengine.periodicaction.PeriodicAction. @author antness */
class PeriodicAction : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/skillengine/periodicaction/PeriodicAction.xml.inc"
public:
};

} // namespace aion::gameserver::skillengine::periodicaction
