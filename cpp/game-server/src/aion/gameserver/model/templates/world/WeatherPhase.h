#pragma once

#include <string_view>

#include "aion/gameserver/model/templates/world/WeatherPhase.xml.h"
#include "aion/gameserver/model/templates/world/fwd.h"

namespace aion::gameserver::model::templates::world {

/**
 * A weather a weather may lead into: its sign (the lead-in), its remain (the tail) or one of its alternatives. The name is resolved to the
 * weather itself when the table is loaded.
 * <p>
 * Java com.aionemu.gameserver.model.templates.world.WeatherPhase (upstream 374336b3b).
 */
class WeatherPhase : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/world/WeatherPhase.xml.inc"
private:
	const WeatherEntry* weather = nullptr; // Java @XmlTransient

public:
	/**
	 * @return The weather of the phase, resolved when the table is loaded.
	 */
	const WeatherEntry* getWeather() const { return weather; }

	/** Java package-private resolve(WeatherTable, WeatherEntry, String), called by WeatherEntry::resolveReferences */
	void resolve(xml::LoadContext& ctx, const WeatherTable& table, const WeatherEntry& owner, std::string_view role);
};

} // namespace aion::gameserver::model::templates::world
