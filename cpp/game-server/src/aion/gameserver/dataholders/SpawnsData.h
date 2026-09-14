#pragma once

#include "aion/gameserver/dataholders/SpawnsData.xml.h"

namespace aion::gameserver::dataholders {

/** Java com.aionemu.gameserver.dataholders.SpawnsData. @author xTz, Rolandas, Neon */
class SpawnsData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/SpawnsData.xml.inc"
public:
};

/** Java com.aionemu.gameserver.dataholders.SpawnsData.UnprocessedSpawns. */
class SpawnsData::UnprocessedSpawns : public ::aion::gameserver::dataholders::SpawnsData {
#include "aion/gameserver/dataholders/SpawnsData_UnprocessedSpawns.xml.inc"
public:
};

} // namespace aion::gameserver::dataholders
