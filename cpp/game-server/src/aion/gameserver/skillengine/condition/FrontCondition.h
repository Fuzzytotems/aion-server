#pragma once

#include "aion/gameserver/skillengine/condition/FrontCondition.xml.h"

namespace aion::gameserver::skillengine::condition {

/** Java com.aionemu.gameserver.skillengine.condition.FrontCondition. @author Rolandas */
class FrontCondition : public ::aion::gameserver::skillengine::condition::Condition {
#include "aion/gameserver/skillengine/condition/FrontCondition.xml.inc"
public:
};

} // namespace aion::gameserver::skillengine::condition
