#pragma once

#include "aion/gameserver/skillengine/condition/BackCondition.xml.h"

namespace aion::gameserver::skillengine::condition {

/** Java com.aionemu.gameserver.skillengine.condition.BackCondition. @author kecimis */
class BackCondition : public ::aion::gameserver::skillengine::condition::Condition {
#include "aion/gameserver/skillengine/condition/BackCondition.xml.inc"
public:
};

} // namespace aion::gameserver::skillengine::condition
