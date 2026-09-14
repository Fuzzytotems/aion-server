#pragma once

#include "aion/gameserver/skillengine/condition/ChargeArmorCondition.xml.h"

namespace aion::gameserver::skillengine::condition {

/** Java com.aionemu.gameserver.skillengine.condition.ChargeArmorCondition. @author Rolandas, Cheatkiller */
class ChargeArmorCondition : public ::aion::gameserver::skillengine::condition::ChargeCondition {
#include "aion/gameserver/skillengine/condition/ChargeArmorCondition.xml.inc"
public:
};

} // namespace aion::gameserver::skillengine::condition
