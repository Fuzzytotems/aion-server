#pragma once

#include <cstdint>

#include "aion/gameserver/model/templates/siegelocation/SiegeLocationTemplate.xml.h"
#include "aion/gameserver/model/templates/L10n.h"

namespace aion::gameserver::model::templates::siegelocation {

/** Java com.aionemu.gameserver.model.templates.siegelocation.SiegeLocationTemplate. @author Sarynth, antness, Source, Wakizashi */
class SiegeLocationTemplate : public ::aion::gameserver::runtime::StaticTemplate, public ::aion::gameserver::model::templates::L10n {
#include "aion/gameserver/model/templates/siegelocation/SiegeLocationTemplate.xml.inc"
public:
	int32_t getL10nId() const override { return nameId; }
};

} // namespace aion::gameserver::model::templates::siegelocation
