#pragma once

#include "aion/gameserver/model/templates/item/RandomItem.xml.h"

namespace aion::gameserver::model::templates::item {

/** Java com.aionemu.gameserver.model.templates.item.RandomItem. @author vlog, Neon */
class RandomItem : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/item/RandomItem.xml.inc"
public:
};

} // namespace aion::gameserver::model::templates::item
