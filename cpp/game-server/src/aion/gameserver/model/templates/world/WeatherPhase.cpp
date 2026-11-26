#include "aion/gameserver/model/templates/world/WeatherPhase.h"

#include <string>

#include "aion/gameserver/dataholders/loadingutils/LoadContext.h"
#include "aion/gameserver/model/templates/world/WeatherEntry.h"
#include "aion/gameserver/model/templates/world/WeatherTable.h"

namespace aion::gameserver::model::templates::world {

// Java WeatherPhase.java:61-67
void WeatherPhase::resolve(xml::LoadContext& ctx, const WeatherTable& table, const WeatherEntry& owner, std::string_view role) {
	weather = table.getEntry(std::string_view(name));
	if (weather == nullptr)
		ctx.fail("Weather " + owner.getName() + " refers to unknown " + std::string(role) + " weather " + name);
	if (weather->getZoneId() != owner.getZoneId())
		ctx.fail("Weather " + owner.getName() + " and its " + std::string(role) + " weather " + name + " belong to different zones");
}

} // namespace aion::gameserver::model::templates::world
