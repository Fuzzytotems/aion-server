#pragma once

#include "aion/gameserver/skillengine/condition/WeaponCondition.xml.h"

namespace aion::gameserver::skillengine::condition {

/** Java com.aionemu.gameserver.skillengine.condition.WeaponCondition. @author ATracer */
class WeaponCondition : public ::aion::gameserver::skillengine::condition::Condition {
#include "aion/gameserver/skillengine/condition/WeaponCondition.xml.inc"
public:
};

} // namespace aion::gameserver::skillengine::condition
