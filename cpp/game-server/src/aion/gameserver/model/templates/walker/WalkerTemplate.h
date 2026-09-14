#pragma once

#include "aion/gameserver/model/templates/walker/WalkerTemplate.xml.h"

namespace aion::gameserver::model::templates::walker {

/** Java com.aionemu.gameserver.model.templates.walker.WalkerTemplate. @author KKnD */
class WalkerTemplate : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/walker/WalkerTemplate.xml.inc"
public:
};

} // namespace aion::gameserver::model::templates::walker
