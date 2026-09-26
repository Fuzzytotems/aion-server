#pragma once

#include "aion/gameserver/skillengine/periodicaction/PeriodicAction.xml.h"

#include "aion/gameserver/skillengine/model/fwd.h"

namespace aion::gameserver::skillengine::periodicaction {

/** Java com.aionemu.gameserver.skillengine.periodicaction.PeriodicAction. @author antness */
class PeriodicAction : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/skillengine/periodicaction/PeriodicAction.xml.inc"
public:
	virtual void act(model::Effect& effect) const = 0;
};

} // namespace aion::gameserver::skillengine::periodicaction
