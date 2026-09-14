#pragma once

#include <cstdint>

#include "aion/gameserver/model/templates/factions/NpcFactionTemplate.xml.h"
#include "aion/gameserver/model/templates/L10n.h"

namespace aion::gameserver::model::templates::factions {

/** Java com.aionemu.gameserver.model.templates.factions.NpcFactionTemplate. @author vlog */
class NpcFactionTemplate : public ::aion::gameserver::runtime::StaticTemplate, public ::aion::gameserver::model::templates::L10n {
#include "aion/gameserver/model/templates/factions/NpcFactionTemplate.xml.inc"
public:
	int32_t getL10nId() const override { return nameId; }
};

} // namespace aion::gameserver::model::templates::factions
