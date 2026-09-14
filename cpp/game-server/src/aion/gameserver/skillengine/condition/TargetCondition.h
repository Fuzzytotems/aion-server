#pragma once

#include "aion/gameserver/skillengine/condition/TargetCondition.xml.h"

namespace aion::gameserver::skillengine::condition {

/** Java com.aionemu.gameserver.skillengine.condition.TargetCondition. @author ATracer, kecimis */
class TargetCondition : public ::aion::gameserver::skillengine::condition::Condition {
#include "aion/gameserver/skillengine/condition/TargetCondition.xml.inc"
public:
};

} // namespace aion::gameserver::skillengine::condition
