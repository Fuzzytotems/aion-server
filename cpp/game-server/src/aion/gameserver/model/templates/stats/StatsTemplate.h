#pragma once

#include "aion/gameserver/model/templates/stats/StatsTemplate.xml.h"

namespace aion::gameserver::model::templates::stats {

/** Java com.aionemu.gameserver.model.templates.stats.StatsTemplate. @author Aquanox, Estrayl, Neon */
class StatsTemplate : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/stats/StatsTemplate.xml.inc"
public:
};

} // namespace aion::gameserver::model::templates::stats
