#pragma once

#include "aion/gameserver/model/templates/item/ResultedItem.xml.h"

namespace aion::gameserver::model::templates::item {

/** Java com.aionemu.gameserver.model.templates.item.ResultedItem. @author antness, Neon */
class ResultedItem : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/item/ResultedItem.xml.inc"
public:
};

} // namespace aion::gameserver::model::templates::item
