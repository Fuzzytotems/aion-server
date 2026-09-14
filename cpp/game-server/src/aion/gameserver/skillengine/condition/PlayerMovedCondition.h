#pragma once

#include "aion/gameserver/skillengine/condition/PlayerMovedCondition.xml.h"

namespace aion::gameserver::skillengine::condition {

/** Java com.aionemu.gameserver.skillengine.condition.PlayerMovedCondition. @author ATracer */
class PlayerMovedCondition : public ::aion::gameserver::skillengine::condition::Condition {
#include "aion/gameserver/skillengine/condition/PlayerMovedCondition.xml.inc"
public:
};

} // namespace aion::gameserver::skillengine::condition
