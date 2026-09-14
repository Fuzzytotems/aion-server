#pragma once

#include "aion/gameserver/skillengine/condition/TargetFlyingCondition.xml.h"

namespace aion::gameserver::skillengine::condition {

/** Java com.aionemu.gameserver.skillengine.condition.TargetFlyingCondition. @author Sippolo, kecimis */
class TargetFlyingCondition : public ::aion::gameserver::skillengine::condition::Condition {
#include "aion/gameserver/skillengine/condition/TargetFlyingCondition.xml.inc"
public:
};

} // namespace aion::gameserver::skillengine::condition
