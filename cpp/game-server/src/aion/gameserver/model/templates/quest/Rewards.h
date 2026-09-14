#pragma once

#include "aion/gameserver/model/templates/quest/Rewards.xml.h"

namespace aion::gameserver::model::templates::quest {

/** Java com.aionemu.gameserver.model.templates.quest.Rewards. */
class Rewards : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/quest/Rewards.xml.inc"
public:
};

} // namespace aion::gameserver::model::templates::quest
