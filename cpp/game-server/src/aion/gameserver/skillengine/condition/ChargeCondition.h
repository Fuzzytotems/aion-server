#pragma once

#include "aion/gameserver/skillengine/condition/ChargeCondition.xml.h"

namespace aion::gameserver::skillengine::condition {

/** Java com.aionemu.gameserver.skillengine.condition.ChargeCondition. @author Rolandas */
class ChargeCondition : public ::aion::gameserver::skillengine::condition::Condition {
#include "aion/gameserver/skillengine/condition/ChargeCondition.xml.inc"
public:
};

} // namespace aion::gameserver::skillengine::condition
