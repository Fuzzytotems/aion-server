#pragma once

#include "aion/gameserver/model/templates/teleport/TelelocationTemplate.xml.h"

namespace aion::gameserver::model::templates::teleport {

/** Java com.aionemu.gameserver.model.templates.teleport.TelelocationTemplate. @author orz */
class TelelocationTemplate : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/teleport/TelelocationTemplate.xml.inc"
public:
};

} // namespace aion::gameserver::model::templates::teleport
