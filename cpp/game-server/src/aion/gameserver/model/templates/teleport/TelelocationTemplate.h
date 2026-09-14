#pragma once

#include <cstdint>

#include "aion/gameserver/model/templates/teleport/TelelocationTemplate.xml.h"
#include "aion/gameserver/model/templates/L10n.h"

namespace aion::gameserver::model::templates::teleport {

/** Java com.aionemu.gameserver.model.templates.teleport.TelelocationTemplate. @author orz */
class TelelocationTemplate : public ::aion::gameserver::runtime::StaticTemplate, public ::aion::gameserver::model::templates::L10n {
#include "aion/gameserver/model/templates/teleport/TelelocationTemplate.xml.inc"
public:
	int32_t getL10nId() const override { return nameId; }
};

} // namespace aion::gameserver::model::templates::teleport
