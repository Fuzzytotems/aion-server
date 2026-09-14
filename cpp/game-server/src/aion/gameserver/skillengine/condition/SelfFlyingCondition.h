#pragma once

#include "aion/gameserver/skillengine/condition/SelfFlyingCondition.xml.h"

namespace aion::gameserver::skillengine::condition {

/** Java com.aionemu.gameserver.skillengine.condition.SelfFlyingCondition. @author kecimis */
class SelfFlyingCondition : public ::aion::gameserver::skillengine::condition::Condition {
#include "aion/gameserver/skillengine/condition/SelfFlyingCondition.xml.inc"
public:
};

} // namespace aion::gameserver::skillengine::condition
