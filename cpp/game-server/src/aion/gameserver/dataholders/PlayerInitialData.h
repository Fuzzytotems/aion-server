#pragma once

#include "aion/gameserver/dataholders/PlayerInitialData.xml.h"

namespace aion::gameserver::dataholders {

/** Java com.aionemu.gameserver.dataholders.PlayerInitialData. @author Aquanox */
class PlayerInitialData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/PlayerInitialData.xml.inc"
public:
};

/** Java com.aionemu.gameserver.dataholders.PlayerInitialData.LocationData. */
class PlayerInitialData::LocationData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/PlayerInitialData_LocationData.xml.inc"
public:
};

/** Java com.aionemu.gameserver.dataholders.PlayerInitialData.PlayerCreationData. */
class PlayerInitialData::PlayerCreationData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/PlayerInitialData_PlayerCreationData.xml.inc"
public:
};

/** Java com.aionemu.gameserver.dataholders.PlayerInitialData.PlayerCreationData.ItemType. */
class PlayerInitialData::PlayerCreationData::ItemType : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/PlayerInitialData_PlayerCreationData_ItemType.xml.inc"
public:
};

} // namespace aion::gameserver::dataholders
