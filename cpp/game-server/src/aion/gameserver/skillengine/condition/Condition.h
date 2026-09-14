#pragma once

#include "aion/gameserver/skillengine/condition/Condition.xml.h"

namespace aion::gameserver::skillengine::condition {

/** Java com.aionemu.gameserver.skillengine.condition.Condition. @author ATracer */
class Condition : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/skillengine/condition/Condition.xml.inc"
public:
};

} // namespace aion::gameserver::skillengine::condition
