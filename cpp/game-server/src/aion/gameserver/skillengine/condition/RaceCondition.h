#pragma once

#include "aion/gameserver/skillengine/condition/RaceCondition.xml.h"

namespace aion::gameserver::skillengine::condition {

/** Java com.aionemu.gameserver.skillengine.condition.RaceCondition. @author kecimis */
class RaceCondition : public ::aion::gameserver::skillengine::condition::Condition {
#include "aion/gameserver/skillengine/condition/RaceCondition.xml.inc"
public:
};

} // namespace aion::gameserver::skillengine::condition
