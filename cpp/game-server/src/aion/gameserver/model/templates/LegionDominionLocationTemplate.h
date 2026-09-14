#pragma once

#include <cstdint>

#include "aion/gameserver/model/templates/LegionDominionLocationTemplate.xml.h"
#include "aion/gameserver/model/templates/L10n.h"

namespace aion::gameserver::model::templates {

/** Java com.aionemu.gameserver.model.templates.LegionDominionLocationTemplate. @author Yeats, Sykra */
class LegionDominionLocationTemplate : public ::aion::gameserver::runtime::StaticTemplate, public ::aion::gameserver::model::templates::L10n {
#include "aion/gameserver/model/templates/LegionDominionLocationTemplate.xml.inc"
public:
	int32_t getL10nId() const override { return nameId; }
};

} // namespace aion::gameserver::model::templates
