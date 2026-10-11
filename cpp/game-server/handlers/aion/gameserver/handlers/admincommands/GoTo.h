#pragma once

#include <initializer_list>
#include <string>
#include <utility>
#include <vector>

#include "aion/gameserver/handlers/admincommands/AdminCommandsPrelude.h"
#include "aion/gameserver/runtime/collections/ArrayList.h"
#include "aion/gameserver/runtime/collections/HashMap.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"

namespace aion::gameserver::handlers::admincommands {

/**
 * //goto: teleports the GM to a named location.
 *
 * @author Dwarfpicker, Imaginary, Neon
 */
class GoTo : public AdminCommand {
public:
	GoTo();

	void execute(Player& player, std::span<const std::string> params) override;

private:
	class Location : public runtime::RefCounted {
	public:
		Location(WorldMapType mapType, float x, float y, float z, int8_t h, std::initializer_list<std::string> identifiers);

		static runtime::Ref<Location> create(WorldMapType mapType, float x, float y, float z, int8_t h, std::initializer_list<std::string> identifiers);

		~Location() override;

		// Java: private fields of the nested class, read by GoTo
		const WorldMapType mapType;
		const float x, y, z;
		const int8_t h;
		runtime::ArrayList<std::string> identifiers{AION_LOCK_CLASS(GoTo::Location::identifiers)};
	};

	/** Java: a local LinkedHashMap&lt;String, Location&gt; (insertion order) */
	using LocationMatches = std::vector<std::pair<std::string, runtime::Ptr<Location>>>;

	bool isSingleSureMatch(const LocationMatches& matches, const std::string& query);

	LocationMatches findPossibleMatch(const std::string& destination);

	static void goTo(Player& player, Location& location);

	void listLocations(Player& player);

	void appendLocationsForMap(std::string& sb, WorldMapType worldMapType, const std::vector<runtime::Ptr<Location>>& locs);

	void appendLocNames(std::string& sb, const runtime::ArrayList<std::string>& locNames);

	void addLocations();

	void addLocation(WorldMapType mapType, float x, float y, float z, std::initializer_list<std::string> identifiers);

	void addLocation(WorldMapType mapType, float x, float y, float z, int8_t h, std::initializer_list<std::string> identifiers);

	runtime::LinkedHashMap<std::string, runtime::Ref<Location>> locations{AION_LOCK_CLASS(GoTo::locations)};
};

} // namespace aion::gameserver::handlers::admincommands
