#pragma once

#include <cstdint>

#include "aion/gameserver/model/templates/world/WorldMapTemplate.xml.h"
#include "aion/gameserver/model/templates/L10n.h"

namespace aion::gameserver::model::templates::world {

/** Java com.aionemu.gameserver.model.templates.world.WorldMapTemplate. @author Luno */
class WorldMapTemplate : public ::aion::gameserver::runtime::StaticTemplate, public ::aion::gameserver::model::templates::L10n {
#include "aion/gameserver/model/templates/world/WorldMapTemplate.xml.inc"
public:
	int32_t getL10nId() const override { return nameId; }
};

} // namespace aion::gameserver::model::templates::world
