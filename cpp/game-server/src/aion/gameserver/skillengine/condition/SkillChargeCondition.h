#pragma once

#include "aion/gameserver/skillengine/condition/SkillChargeCondition.xml.h"

namespace aion::gameserver::skillengine::condition {

/** Java com.aionemu.gameserver.skillengine.condition.SkillChargeCondition. @author Rolandas */
class SkillChargeCondition : public ::aion::gameserver::skillengine::condition::ChargeCondition {
#include "aion/gameserver/skillengine/condition/SkillChargeCondition.xml.inc"
public:
};

} // namespace aion::gameserver::skillengine::condition
