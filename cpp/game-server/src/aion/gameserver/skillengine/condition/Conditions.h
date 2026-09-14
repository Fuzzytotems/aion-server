#pragma once

#include "aion/gameserver/skillengine/condition/Conditions.xml.h"

namespace aion::gameserver::skillengine::condition {

/** Java com.aionemu.gameserver.skillengine.condition.Conditions. @author ATracer */
class Conditions : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/skillengine/condition/Conditions.xml.inc"
public:
};

} // namespace aion::gameserver::skillengine::condition
