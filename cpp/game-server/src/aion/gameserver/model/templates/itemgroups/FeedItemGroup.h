#pragma once

#include "aion/gameserver/model/templates/itemgroups/FeedItemGroup.xml.h"

namespace aion::gameserver::model::templates::itemgroups {

/** Java com.aionemu.gameserver.model.templates.itemgroups.FeedItemGroup. @author Rolandas */
class FeedItemGroup : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/itemgroups/FeedItemGroup.xml.inc"
public:
};

} // namespace aion::gameserver::model::templates::itemgroups
