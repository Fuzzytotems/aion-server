#pragma once

#include "aion/gameserver/skillengine/model/MotionTime.xml.h"

namespace aion::gameserver::skillengine::model {

/** Java com.aionemu.gameserver.skillengine.model.MotionTime. */
class MotionTime : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/skillengine/model/MotionTime.xml.inc"
public:
};

} // namespace aion::gameserver::skillengine::model
