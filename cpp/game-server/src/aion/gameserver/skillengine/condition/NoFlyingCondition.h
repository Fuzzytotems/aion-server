#pragma once

#include "aion/gameserver/skillengine/condition/NoFlyingCondition.xml.h"

namespace aion::gameserver::skillengine::condition {

/** Java com.aionemu.gameserver.skillengine.condition.NoFlyingCondition. @author Sippolo */
class NoFlyingCondition : public ::aion::gameserver::skillengine::condition::Condition {
#include "aion/gameserver/skillengine/condition/NoFlyingCondition.xml.inc"
public:
};

} // namespace aion::gameserver::skillengine::condition
