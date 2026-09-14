#pragma once

#include "aion/gameserver/dataholders/ChallengeData.xml.h"

namespace aion::gameserver::dataholders {

/** Java com.aionemu.gameserver.dataholders.ChallengeData. @author ViAl */
class ChallengeData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/ChallengeData.xml.inc"
public:
};

} // namespace aion::gameserver::dataholders
