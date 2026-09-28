#pragma once

#include <cstdint>

#include "aion/gameserver/model/templates/factions/NpcFactionTemplate.xml.h"
#include "aion/gameserver/model/templates/L10n.h"
#include "aion/gameserver/runtime/base/Exceptions.h"

namespace aion::gameserver::model::templates::factions {

/** Java com.aionemu.gameserver.model.templates.factions.NpcFactionTemplate. @author vlog */
class NpcFactionTemplate : public ::aion::gameserver::runtime::StaticTemplate, public ::aion::gameserver::model::templates::L10n {
#include "aion/gameserver/model/templates/factions/NpcFactionTemplate.xml.inc"
public:
	int32_t getL10nId() const override { return nameId; }

	/** Java unboxes the Integer minLevel: NullPointerException for a faction without min_level */
	int32_t getMinLevel() const {
		if (!minLevel)
			throw ::aion::gameserver::runtime::NullPointerException("NpcFactionTemplate.minLevel is null");
		return *minLevel;
	}

	bool isMentor() const { return category == FactionCategory::MENTOR; }
};

} // namespace aion::gameserver::model::templates::factions
