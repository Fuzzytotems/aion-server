#pragma once

#include <cstdint>

#include "aion/gameserver/model/autogroup/AutoGroup.xml.h"
#include "aion/gameserver/model/templates/L10n.h"

namespace aion::gameserver::model::autogroup {

/** Java com.aionemu.gameserver.model.autogroup.AutoGroup. @author MrPoke */
class AutoGroup : public ::aion::gameserver::runtime::StaticTemplate, public ::aion::gameserver::model::templates::L10n {
#include "aion/gameserver/model/autogroup/AutoGroup.xml.inc"
public:
	int32_t getL10nId() const override { return nameId; }
};

} // namespace aion::gameserver::model::autogroup
