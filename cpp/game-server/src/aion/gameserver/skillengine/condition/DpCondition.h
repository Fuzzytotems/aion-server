#pragma once

#include "aion/gameserver/skillengine/condition/DpCondition.xml.h"

namespace aion::gameserver::skillengine::condition {

/** Java com.aionemu.gameserver.skillengine.condition.DpCondition. @author ATracer */
class DpCondition : public ::aion::gameserver::skillengine::condition::Condition {
#include "aion/gameserver/skillengine/condition/DpCondition.xml.inc"
public:
};

} // namespace aion::gameserver::skillengine::condition
