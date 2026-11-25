#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "aion/gameserver/model/templates/world/WeatherEntry.xml.h"
#include "aion/gameserver/model/templates/world/fwd.h"

namespace aion::gameserver::model::templates::world {

/**
 * One weather of a map: when it may start, how long it and its phases last, and which weathers may precede and follow it.
 * <p>
 * Java com.aionemu.gameserver.model.templates.world.WeatherEntry (reworked by upstream 374336b3b).
 *
 * @author Rolandas
 */
class WeatherEntry : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/world/WeatherEntry.xml.inc"
private:
	static constexpr int32_t ALL_MONTHS = (1 << 12) - 1;

	int32_t monthMask = 0; // Java @XmlTransient
	int32_t hourMask = 0; // Java @XmlTransient

public:
	/**
	 * Parses {@code "1,3-5,12"}. {@code shift} maps a value to its bit: months are stored from 1, hours from 0.
	 * <p>
	 * C++: Java's null spec is std::nullopt; a malformed number throws like Integer.parseInt (NumberFormatException).
	 */
	static int32_t parseMask(const std::optional<std::string>& spec, int32_t shift);

	/**
	 * @param month Month of the game calendar, 0-11.
	 * @param hour  Hour of the game day, 0-23.
	 */
	bool isDueIn(int32_t month, int32_t hour) const;

	/**
	 * @return Alternative weathers which may follow the main phase, in the order they are rolled for, never null.
	 */
	const std::vector<WeatherPhase>& getSubWeathers() const { return subWeathers; }

	/**
	 * Resolves the phases against the weathers of the same zone. A world whose weather refers to a weather of another zone or to an unknown name
	 * does not load. (Java package-private, called by WeatherTable::afterUnmarshal)
	 */
	void resolveReferences(xml::LoadContext& ctx, const WeatherTable& table);
};

} // namespace aion::gameserver::model::templates::world
