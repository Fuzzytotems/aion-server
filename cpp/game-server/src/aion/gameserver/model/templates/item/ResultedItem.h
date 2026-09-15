#pragma once

#include "aion/gameserver/model/templates/item/ResultedItem.xml.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"

namespace aion::gameserver::model::templates::item {

/** Java com.aionemu.gameserver.model.templates.item.ResultedItem. @author antness, Neon */
class ResultedItem : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/item/ResultedItem.xml.inc"
public:
	bool isObtainableFor(gameobjects::player::Player& player) const;
};

} // namespace aion::gameserver::model::templates::item
