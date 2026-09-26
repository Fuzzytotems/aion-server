#pragma once

#include "aion/gameserver/skillengine/periodicaction/HpUsePeriodicAction.xml.h"

#include "aion/gameserver/skillengine/model/fwd.h"

namespace aion::gameserver::skillengine::periodicaction {

/** Java com.aionemu.gameserver.skillengine.periodicaction.HpUsePeriodicAction. @author antness */
class HpUsePeriodicAction : public ::aion::gameserver::skillengine::periodicaction::PeriodicAction {
#include "aion/gameserver/skillengine/periodicaction/HpUsePeriodicAction.xml.inc"
public:
	void act(model::Effect& effect) const override;
};

} // namespace aion::gameserver::skillengine::periodicaction
