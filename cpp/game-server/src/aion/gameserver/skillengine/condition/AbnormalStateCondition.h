#pragma once

#include "aion/gameserver/skillengine/condition/AbnormalStateCondition.xml.h"

namespace aion::gameserver::skillengine::condition {

/** Java com.aionemu.gameserver.skillengine.condition.AbnormalStateCondition. @author kecimis */
class AbnormalStateCondition : public ::aion::gameserver::skillengine::condition::Condition {
#include "aion/gameserver/skillengine/condition/AbnormalStateCondition.xml.inc"
public:
};

} // namespace aion::gameserver::skillengine::condition
