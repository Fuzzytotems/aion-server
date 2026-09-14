#pragma once

#include "aion/gameserver/skillengine/condition/ChargeWeaponCondition.xml.h"

namespace aion::gameserver::skillengine::condition {

/** Java com.aionemu.gameserver.skillengine.condition.ChargeWeaponCondition. @author Rolandas, Cheatkiller */
class ChargeWeaponCondition : public ::aion::gameserver::skillengine::condition::ChargeCondition {
#include "aion/gameserver/skillengine/condition/ChargeWeaponCondition.xml.inc"
public:
};

} // namespace aion::gameserver::skillengine::condition
