#pragma once

#include "aion/gameserver/skillengine/condition/MpCondition.xml.h"

namespace aion::gameserver::skillengine::condition {

/** Java com.aionemu.gameserver.skillengine.condition.MpCondition. @author ATracer */
class MpCondition : public ::aion::gameserver::skillengine::condition::Condition {
#include "aion/gameserver/skillengine/condition/MpCondition.xml.inc"
public:
};

} // namespace aion::gameserver::skillengine::condition
