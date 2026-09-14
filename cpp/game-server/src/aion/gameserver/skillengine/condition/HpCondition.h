#pragma once

#include "aion/gameserver/skillengine/condition/HpCondition.xml.h"

namespace aion::gameserver::skillengine::condition {

/** Java com.aionemu.gameserver.skillengine.condition.HpCondition. @author Tomate */
class HpCondition : public ::aion::gameserver::skillengine::condition::Condition {
#include "aion/gameserver/skillengine/condition/HpCondition.xml.inc"
public:
};

} // namespace aion::gameserver::skillengine::condition
