#pragma once

#include "aion/gameserver/model/templates/item/AssemblyItem.xml.h"

namespace aion::gameserver::model::templates::item {

/** Java com.aionemu.gameserver.model.templates.item.AssemblyItem. @author xTz */
class AssemblyItem : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/item/AssemblyItem.xml.inc"
public:
};

} // namespace aion::gameserver::model::templates::item
