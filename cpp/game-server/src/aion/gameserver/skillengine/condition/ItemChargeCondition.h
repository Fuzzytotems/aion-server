#pragma once

#include "aion/gameserver/skillengine/condition/ItemChargeCondition.xml.h"

namespace aion::gameserver::skillengine::condition {

/** Java com.aionemu.gameserver.skillengine.condition.ItemChargeCondition. @author Rolandas */
class ItemChargeCondition : public ::aion::gameserver::skillengine::condition::ChargeCondition {
#include "aion/gameserver/skillengine/condition/ItemChargeCondition.xml.inc"
public:
};

} // namespace aion::gameserver::skillengine::condition
