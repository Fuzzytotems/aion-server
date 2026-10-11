#include "aion/gameserver/handlers/admincommands/GoTo.h"

#include <algorithm>

#include "aion/commons/utils/StringUtils.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/services/instance/InstanceService.h"
#include "aion/gameserver/services/teleport/TeleportService.h"
#include "aion/gameserver/utils/ChatUtil.h"
#include "aion/gameserver/utils/JavaColor.h"
#include "aion/gameserver/world/World.h"
#include "aion/gameserver/world/WorldMap.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldMapTypeInfo.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::handlers::admincommands {

AION_ADMIN_COMMAND(GoTo);

// parity= private final Map<String, Location> locations = new LinkedHashMap<>(); (GoTo.h)

// Java GoTo.java:26-32
GoTo::GoTo()
	: AdminCommand("goto", "Teleports you to regions by name.",
		  " - Shows a list of locations to teleport to.\n"
		  "<location name> - Teleports you to the given location.\n") {
	addLocations();
}

// Java GoTo.java:34-61
void GoTo::execute(Player& player, std::span<const std::string> params) {
	if (params.size() == 0) { // parity= if (params.length == 0) {
		listLocations(player);
		return;
	}

	std::string destination = commons::utils::StringUtils::toLowerCase(commons::utils::StringUtils::trim(join(params, 0))); // parity= String destination = String.join(" ", params).trim().toLowerCase();
	runtime::Ptr<Location> location = locations.get(destination);
	if (location == nullptr) {
		LocationMatches matches = findPossibleMatch(destination); // parity= Map<String, Location> matches = findPossibleMatch(destination);
		if (isSingleSureMatch(matches, destination)) {
			location = matches.front().second; // parity= location = matches.values().iterator().next();
		} else {
			std::string msg = "Could not find the specified destination."; // parity= StringBuilder msg = new StringBuilder("Could not find the specified destination.");
			if (!matches.empty()) {
				msg += " Possible matches:"; // parity= msg.append(" Possible matches:");
				for (const auto& [match, loc] : matches) // parity= matches.forEach((match, loc) -> msg.append("\n\t- ").append(ChatUtil.color(match, Color.WHITE)).append(" (").append(worldName(loc.mapType.getId())).append(")"));
					msg += "\n\t- " + ChatUtil::color(match, utils::JavaColor::WHITE) + " (" + worldName(world::getId(loc->mapType)) + ")"; // parity: (the same)
			}
			sendInfo(player, msg); // parity= sendInfo(player, msg.toString());
		}
		if (location == nullptr)
			return;
	}

	goTo(player, *location);
}

// Java GoTo.java:63-65
bool GoTo::isSingleSureMatch(const LocationMatches& matches, const std::string& query) {
	return matches.size() == 1 && // parity= return matches.size() == 1 && (query.length() >= 5 || matches.keySet().iterator().next().toLowerCase().startsWith(query));
		(commons::utils::StringUtils::utf16Length(query) >= 5 || commons::utils::StringUtils::toLowerCase(matches.front().first).starts_with(query)); // parity: (the same)
}

// Java GoTo.java:67-80
GoTo::LocationMatches GoTo::findPossibleMatch(const std::string& destination) {
	LocationMatches possibleMatches; // parity= Map<String, Location> possibleMatches = new LinkedHashMap<>();
	if (commons::utils::StringUtils::utf16Length(destination) >= 2) { // parity= if (destination.length() >= 2) {
		for (const runtime::Ptr<Location>& loc : locations.values()) { // parity= for (Location loc : locations.values()) {
			for (const std::string& identifier : loc->identifiers) { // parity= for (String identifier : loc.identifiers) {
				if (commons::utils::StringUtils::toLowerCase(identifier).find(destination) != std::string::npos) { // parity= if (identifier.toLowerCase().contains(destination)) {
					// Java: possibleMatches.put(identifier, loc) - a known key keeps its place (and its location: the same one)
					if (std::none_of(possibleMatches.begin(), possibleMatches.end(), [&identifier](const auto& entry) { return entry.first == identifier; })) // parity: (put keeps a known key in its place)
						possibleMatches.emplace_back(identifier, loc); // parity= possibleMatches.put(identifier, loc);
					break;
				}
			}
		}
	}
	return possibleMatches;
}

// Java GoTo.java:82-92
void GoTo::goTo(Player& player, Location& location) {
	runtime::Ptr<WorldMapInstance> instance; // parity= WorldMapInstance instance;
	if (player.getWorldId() == world::getId(location.mapType)) // parity= if (player.getWorldId() == location.mapType.getId())
		instance = player.getPosition()->getWorldMapInstance();
	else if (World::getInstance().getWorldMap(world::getId(location.mapType))->isInstanceType()) // parity= else if (World.getInstance().getWorldMap(location.mapType.getId()).isInstanceType())
		instance = InstanceService::getOrRegisterInstance(world::getId(location.mapType), player); // parity= instance = InstanceService.getOrRegisterInstance(location.mapType.getId(), player);
	else
		instance = World::getInstance().getWorldMap(world::getId(location.mapType))->getMainWorldMapInstance(); // parity= instance = World.getInstance().getWorldMap(location.mapType.getId()).getMainWorldMapInstance();
	TeleportService::teleportTo(player, *instance, location.x, location.y, location.z, location.h != 0 ? location.h : player.getHeading(), // parity= TeleportService.teleportTo(player, instance, location.x, location.y, location.z, location.h != 0 ? location.h : player.getHeading(), TeleportAnimation.NONE);
		TeleportAnimation::NONE); // parity: (the same)
}

// Java GoTo.java:94-101
void GoTo::listLocations(Player& player) {
	// Java: groupingBy(mapType, LinkedHashMap::new, mapping(getValue, toCollection(LinkedHashSet::new))) - Location has no equals (identity)
	std::vector<std::pair<WorldMapType, std::vector<runtime::Ptr<Location>>>> locsByWorld; // parity= Map<WorldMapType, Collection<Location>> locsByWorld = locations.entrySet().stream().collect(groupingBy(e -> e.getValue().mapType, LinkedHashMap::new, mapping(Map.Entry::getValue, toCollection(LinkedHashSet::new))));
	for (const auto& entry : locations.entrySet()) { // parity: (groupingBy, above)
		const runtime::Ptr<Location>& loc = entry.getValue(); // parity: (the same)
		auto group = std::find_if(locsByWorld.begin(), locsByWorld.end(), [&loc](const auto& byWorld) { return byWorld.first == loc->mapType; }); // parity: (the same)
		if (group == locsByWorld.end()) // parity: (the same)
			group = locsByWorld.insert(locsByWorld.end(), {loc->mapType, {}}); // parity: (the same)
		if (std::find(group->second.begin(), group->second.end(), loc) == group->second.end()) // parity: (the same)
			group->second.push_back(loc); // parity: (the same)
	}
	std::string sb = "Available locations:"; // parity= StringBuilder sb = new StringBuilder("Available locations:");
	for (const auto& [worldMapType, locs] : locsByWorld) // parity= locsByWorld.forEach((worldMapType, locs) -> appendLocationsForMap(sb, worldMapType, locs));
		appendLocationsForMap(sb, worldMapType, locs); // parity: (the same)
	sb += "\nType " + ChatUtil::color(getAliasWithPrefix() + " <location name>", utils::JavaColor::WHITE) + // parity= sb.append("\nType " + ChatUtil.color(getAliasWithPrefix() + " <location name>", Color.WHITE) + " to teleport to a location. Location names can be abbreviated.");
		" to teleport to a location. Location names can be abbreviated."; // parity: (the same)
	sendInfo(player, sb); // parity= sendInfo(player, sb.toString());
}

// Java GoTo.java:103-113
void GoTo::appendLocationsForMap(std::string& sb, WorldMapType worldMapType, const std::vector<runtime::Ptr<Location>>& locs) {
	sb += '\n'; // parity= sb.append('\n').append(worldName(worldMapType.getId())).append(':');
	sb += worldName(world::getId(worldMapType)); // parity: (the same)
	sb += ':'; // parity: (the same)
	for (const runtime::Ptr<Location>& loc : locs) {
		if (locs.size() > 1)
			sb += '\n'; // parity= sb.append('\n');
		sb += '\t'; // parity= sb.append('\t');
		if (locs.size() > 1)
			sb += "- "; // parity= sb.append("- ");
		appendLocNames(sb, loc->identifiers);
	}
}

// Java GoTo.java:115-121
void GoTo::appendLocNames(std::string& sb, const runtime::ArrayList<std::string>& locNames) {
	for (int32_t i = 0; i < locNames.size();) { // parity= for (int i = 0; i < locNames.size();) {
		sb += ChatUtil::color(locNames.get(i++), utils::JavaColor::WHITE); // parity= sb.append(ChatUtil.color(locNames.get(i++), Color.WHITE));
		if (i != locNames.size())
			sb += " / "; // parity= sb.append(" / ");
	}
}

// Java GoTo.java:123-410
void GoTo::addLocations() {
	// Elysea
	addLocation(WorldMapType::SANCTUM, 1322, 1511, 568, {"Sanctum"});
	addLocation(WorldMapType::KAISINEL, 2155, 1567, 1205, {"Kaisinel", "Kaisinels Kloster"});
	addLocation(WorldMapType::WISPLIGHT_ABBEY, 247, 228, 130, {"Wisplight Abbey", "Zuflucht der Rückkehrer Elyos", "Zuflucht Elyos"});
	addLocation(WorldMapType::POETA, 806, 1242, 119, {"Poeta"});
	addLocation(WorldMapType::POETA, 426, 1740, 119, {"Melponeh", "Melponehs Lager"});
	addLocation(WorldMapType::VERTERON, 1643, 1500, 119, {"Verteron"});
	addLocation(WorldMapType::VERTERON, 2384, 788, 102, {"Cantas Coast", "Kantas Küste"});
	addLocation(WorldMapType::VERTERON, 2333, 1817, 193, {"Ardus Shrine", "Ardus Schrein"});
	addLocation(WorldMapType::VERTERON, 2063, 2412, 274, {"Pilgrims Respite", "Pilgers Rast"});
	addLocation(WorldMapType::VERTERON, 1291, 2206, 142, {"Tolbas Village"});
	addLocation(WorldMapType::ELTNEN, 343, 2724, 264, {"Eltnen"});
	addLocation(WorldMapType::ELTNEN, 688, 431, 332, {"Golden", "Golden Bough Garrison", "Garnison der Goldbogenlegion"});
	addLocation(WorldMapType::ELTNEN, 1779, 883, 422, {"Eltnen Observatory", "Observatorium von Eltnen"});
	addLocation(WorldMapType::ELTNEN, 947, 2215, 252, {"Novan", "Novans Kreuzung"});
	addLocation(WorldMapType::ELTNEN, 1921, 2045, 361, {"Agairon"});
	addLocation(WorldMapType::ELTNEN, 2411, 2724, 361, {"Kuriullu"});
	addLocation(WorldMapType::THEOBOMOS, 1398, 1557, 31, {"Theobomos"});
	addLocation(WorldMapType::THEOBOMOS, 458, 1257, 127, {"Jamanok Inn", "Jamanoks Gasthaus"});
	addLocation(WorldMapType::THEOBOMOS, 1396, 1560, 31, {"Meniherk"});
	addLocation(WorldMapType::THEOBOMOS, 2234, 2284, 50, {"Obsvillage", "Observatoriumsdorf"});
	addLocation(WorldMapType::THEOBOMOS, 901, 2774, 62, {"Josnack", "Josnacks Zelt"});
	addLocation(WorldMapType::THEOBOMOS, 2681, 847, 138, {"Anangke"});
	addLocation(WorldMapType::HEIRON, 2540, 343, 411, {"Heiron"});
	addLocation(WorldMapType::HEIRON, 1423, 1334, 175, {"Heiron Observatory", "Heiron Observatorium"});
	addLocation(WorldMapType::HEIRON, 971, 686, 135, {"Senemonea", "Seneas Lager"});
	addLocation(WorldMapType::HEIRON, 1635, 2693, 115, {"Jeiaparan"});
	addLocation(WorldMapType::HEIRON, 916, 2256, 157, {"Changarnerk", "Changarnerks Lager"});
	addLocation(WorldMapType::HEIRON, 1999, 1391, 118, {"Kishar"});
	addLocation(WorldMapType::HEIRON, 170, 1662, 120, {"Arbolu", "Arbolus Oase"});
	// Asmodae
	addLocation(WorldMapType::PANDAEMONIUM, 1679, 1400, 195, {"Pandaemonium", "Pandämonium"});
	addLocation(WorldMapType::MARCHUTAN, 1557, 1429, 266, {"Marchutan", "Marchutans Konvent"});
	addLocation(WorldMapType::FATEBOUND_ABBEY, 281, 266, 97, {"Fatebound Abbey", "Zuflucht der Rückkehrer Asmo", "Zuflucht Asmo"});
	addLocation(WorldMapType::ISHALGEN, 529, 2449, 281, {"Ishalgen"});
	addLocation(WorldMapType::ISHALGEN, 940, 1707, 259, {"Anturoon", "Anturoon Wachposten"});
	addLocation(WorldMapType::ALTGARD, 1748, 1807, 254, {"Altgard"});
	addLocation(WorldMapType::ALTGARD, 1903, 696, 260, {"Basfelt"});
	addLocation(WorldMapType::ALTGARD, 2680, 1024, 311, {"Trader", "Handelshafen"});
	addLocation(WorldMapType::ALTGARD, 2643, 1658, 324, {"Impetusiom", "Herz des Impetusiums"});
	addLocation(WorldMapType::ALTGARD, 1468, 2560, 299, {"Altgard Observatory", "Observatorium von Altgard"});
	addLocation(WorldMapType::MORHEIM, 308, 2274, 449, {"Morheim"});
	addLocation(WorldMapType::MORHEIM, 634, 900, 360, {"Desert", "Wüstengarnison"});
	addLocation(WorldMapType::MORHEIM, 1772, 1662, 197, {"Slag", "Schlackenbollwerk"});
	addLocation(WorldMapType::MORHEIM, 1070, 2486, 239, {"Kellan", "Kellans Hütte"});
	addLocation(WorldMapType::MORHEIM, 2387, 1742, 102, {"Alsig"});
	addLocation(WorldMapType::MORHEIM, 2794, 1122, 171, {"Morheim Observatory", "Observatorium von Morheim"});
	addLocation(WorldMapType::MORHEIM, 2346, 2219, 127, {"Halabana"});
	addLocation(WorldMapType::BRUSTHONIN, 2917, 2421, 15, {"Brusthonin"});
	addLocation(WorldMapType::BRUSTHONIN, 1413, 2013, 51, {"Baltasar"});
	addLocation(WorldMapType::BRUSTHONIN, 840, 2016, 307, {"Bollu", "Iollu"});
	addLocation(WorldMapType::BRUSTHONIN, 1523, 374, 231, {"Edge", "Saum der Qualen"});
	addLocation(WorldMapType::BRUSTHONIN, 526, 848, 76, {"Bubu", "Bubu Dorf"});
	addLocation(WorldMapType::BRUSTHONIN, 2917, 2417, 15, {"Settlers", "Siedlerlager"});
	addLocation(WorldMapType::BELUSLAN, 398, 400, 222, {"Beluslan"});
	addLocation(WorldMapType::BELUSLAN, 533, 1866, 262, {"Besfer"});
	addLocation(WorldMapType::BELUSLAN, 1243, 819, 260, {"Kidorun", "Kidoruns Lager"});
	addLocation(WorldMapType::BELUSLAN, 2358, 1241, 470, {"Red Mane", "Rotmähnenhöhle"});
	addLocation(WorldMapType::BELUSLAN, 1942, 513, 412, {"Kistenian"});
	addLocation(WorldMapType::BELUSLAN, 2431, 2063, 579, {"Hoarfrost", "Raureif"});
	// Balaurea
	addLocation(WorldMapType::INGGISON, 1335, 276, 590, {"Inggison"});
	addLocation(WorldMapType::INGGISON, 382, 951, 460, {"Ufob", "Undirborg"});
	addLocation(WorldMapType::INGGISON, 2713, 1477, 382, {"Soteria"});
	addLocation(WorldMapType::INGGISON, 1892, 1748, 327, {"Hanarkand"});
	addLocation(WorldMapType::GELKMAROS, 1763, 2911, 554, {"Gelkmaros"});
	addLocation(WorldMapType::GELKMAROS, 2503, 2147, 464, {"Subterranea"});
	addLocation(WorldMapType::GELKMAROS, 845, 1737, 354, {"Rhonnam"});
	addLocation(WorldMapType::GELKMAROS, 1295, 1558, 306, {"Späherposten"});
	addLocation(WorldMapType::SILENTERA_CANYON, 583, 767, 300, {"Silentera"});
	// Abyss
	addLocation(WorldMapType::RESHANTA, 951, 936, 1667, {"Reshanta"});
	addLocation(WorldMapType::RESHANTA, 2867, 1034, 1528, {"Abyss 1"});
	addLocation(WorldMapType::RESHANTA, 1078, 2839, 1636, {"Abyss 2"});
	addLocation(WorldMapType::RESHANTA, 1596, 2952, 2943, {"Abyss 3"});
	addLocation(WorldMapType::RESHANTA, 2054, 660, 2843, {"Abyss 4"});
	addLocation(WorldMapType::RESHANTA, 1979, 2114, 2291, {"Eye of Reshanta"});
	addLocation(WorldMapType::RESHANTA, 2130, 1925, 2322, {"Divine Fortress"});
	// Instances
	addLocation(WorldMapType::HARAMEL, 176, 21, 144, {"Haramel"});
	addLocation(WorldMapType::NOCHSANA_TRAINING_CAMP, 513, 668, 331, {"Nochsana Training Camp", "NTC"});
	addLocation(WorldMapType::ARKANIS_TEMPLE, 177, 229, 536, {"Sky Temple of Arkanis", "Himmelstempel von Arkanis"});
	addLocation(WorldMapType::FIRE_TEMPLE, 144, 312, 123, {"Fire Temple", "FT", "Feuertempel"});
	addLocation(WorldMapType::KROMEDES_TRIAL, 248, 244, 189, {"Kromedes Trial", "Kromedes Prozess"});
	addLocation(WorldMapType::STEEL_RAKE, 237, 506, 948, {"Steel Rake", "SR", "Stahlharke"});
	addLocation(WorldMapType::STEEL_RAKE, 283, 453, 903, {"Steel Rake Lower", "SR Low", "Stahlharke Unten"});
	addLocation(WorldMapType::STEEL_RAKE, 283, 453, 953, {"Steel Rake Middle", "SR Mid", "Stahlharke Mitte"});
	addLocation(WorldMapType::INDRATU_FORTRESS, 562, 335, 1015, {"Indratu Fortress", "Indratu Festung"});
	addLocation(WorldMapType::AZOTURAN_FORTRESS, 458, 428, 1039, {"Azoturan Fortress", "Azoturan Festung"});
	addLocation(WorldMapType::AETHEROGENETICS_LAB, 225, 244, 133, {"Aetherogenetics Lab", "Lepharisten Geheimlabor"});
	addLocation(WorldMapType::ADMA_STRONGHOLD, 450, 200, 168, {"Adma", "Adma Stronghold", "Adma Festung"});
	addLocation(WorldMapType::ALQUIMIA_RESEARCH_CENTER, 603, 527, 200, {"Alquimia Research Center", "Alquimia Labor"});
	addLocation(WorldMapType::DRAUPNIR_CAVE, 491, 373, 622, {"Draupnir Cave", "Draupnir Höhle"});
	addLocation(WorldMapType::THEOBOMOS_LAB, 477, 201, 170, {"Theobomos Lab", "Theobomos Research Lab", "TheoLab", "Theobomos Geheimlabor"});
	addLocation(WorldMapType::DARK_POETA, 1214, 412, 140, {"Dark Poeta", "DP", "Poeta der Finsternis"});
	addLocation(WorldMapType::SHUGO_IMPERIAL_TOMB, 178, 234, 537, {"Shugo Imperial Tomb", "Imperial", "Tomb"});
	// Lower Abyss
	addLocation(WorldMapType::SULFUR_TREE_NEST, 462, 345, 163, {"Sulfur Tree Nest", "Schwefelbaum Nest"});
	addLocation(WorldMapType::RIGHT_WING_CHAMBER, 263, 386, 103, {"Right Wing Chamber", "Rechter Flügel", "Kammer Im Rechten Flügel"});
	addLocation(WorldMapType::LEFT_WING_CHAMBER, 672, 606, 321, {"Left Wing Chamber", "Linker Flügel", "Kammer Im Linken Flügel"});
	// Upper Abyss
	addLocation(WorldMapType::ASTERIA_CHAMBER, 469, 568, 202, {"Asteria Chamber", "Abyss von Asteria"});
	addLocation(WorldMapType::MIREN_CHAMBER, 527, 120, 176, {"Miren Chamber", "Miren Kammer"});
	addLocation(WorldMapType::LEGIONS_MIREN_BARRACKS, 528, 121, 176, {"Miren Legion Barracks", "Miren Legionsfestung"});
	addLocation(WorldMapType::MIREN_BARRACKS, 528, 121, 176, {"Miren Barracks", "Miren Kriegsfestung"});
	addLocation(WorldMapType::KYSIS_CHAMBER, 528, 121, 176, {"Kysis Chamber", "Kysis Kammer"});
	addLocation(WorldMapType::LEGIONS_KYSIS_BARRACKS, 528, 121, 176, {"Kysis Legion Barracks", "Kysis Legionsfestung"});
	addLocation(WorldMapType::KYSIS_BARRACKS, 528, 121, 176, {"Kysis Barracks", "Kysis Kriegsfestung"});
	addLocation(WorldMapType::KROTAN_CHAMBER, 528, 109, 176, {"Krotan Chamber", "Krotan Kammer"});
	addLocation(WorldMapType::LEGIONS_KROTAN_BARRACKS, 528, 121, 176, {"Krotan Legion Barracks", "Krotan Legionsfestung"});
	addLocation(WorldMapType::KROTAN_BARRACKS, 528, 121, 176, {"Krotan Barracks", "Krotan Kriegsfestung"});
	addLocation(WorldMapType::CHAMBER_OF_ROAH, 504, 396, 94, {"Roah Chamber", "Kaverne von Roah"});
	// Divine
	addLocation(WorldMapType::ABYSSAL_SPLINTER, 704, 153, 453, {"Abyssal Splinter", "AS", "Abyss Splitter"});
	addLocation(WorldMapType::UNSTABLE_SPLINTER, 704, 153, 453, {"Unstable Abyssal Splinter", "UAS", "Zerbrochener Abyss Splitter"});
	addLocation(WorldMapType::DREDGION, 414, 193, 431, {"Dredgion"});
	addLocation(WorldMapType::DREDGION_OF_CHANTRA, 414, 193, 431, {"Chantra Dredgion"});
	addLocation(WorldMapType::TERATH_DREDGION, 414, 193, 431, {"Terath Dredgion", "Sadha Dredgion"});
	addLocation(WorldMapType::TALOCS_HOLLOW, 200, 214, 1099, {"Taloc's Hollow", "Talocs Höhle"});
	addLocation(WorldMapType::UDAS_TEMPLE, 637, 657, 134, {"Udas", "Udas Temple", "Udas Tempel"});
	addLocation(WorldMapType::UDAS_TEMPLE_LOWER, 1146, 277, 116, {"Udas Lower Temple", "Udas Tempelgruft"});
	addLocation(WorldMapType::BESHMUNDIR_TEMPLE, 1477, 237, 243, {"Beshmundir Temple", "BT"});
	addLocation(WorldMapType::PADMARASHKA_CAVE, 385, 506, 66, {"Padmarashka Cave"});
	// 3.0 Instances
	addLocation(WorldMapType::ATURAM_SKY_FORTRESS, 691.459f, 456.8719f, 655.7797f, {"ASF", "Aturam Sky Fortress", "Aturam Himmelsfestung"});
	// 4.0 Instances
	addLocation(WorldMapType::SAURO_SUPPLY_BASE, 640.7884f, 174.29156f, 195.625f, {"Sauro Supply Base"});
	addLocation(WorldMapType::ETERNAL_BASTION, 745.86206f, 291.18323f, 233.7940f, {"Eternal Bastion", "EB", "Stahlmauerbastion"});
	addLocation(WorldMapType::SEALED_DANUAR_MYSTICARIUM, 184.35f, 121.3f, 231.3f, {"Danuar Mysticarium", "DM", "Versiegelte Halle des Wissens", "VHDW"});
	addLocation(WorldMapType::OPHIDAN_BRIDGE, 755.41864f, 560.617f, 572.9637f, {"Ophidan Bridge", "OB", "Jormungand Brücke"});
	addLocation(WorldMapType::DANUAR_RELIQUARY, 256.60f, 257.99f, 241.78f, {"Danuar Reliquary", "DR", "Ruhnadium"});
	addLocation(WorldMapType::DANUAR_SANCTUARY, 388.66f, 1185.07f, 55.31f, {"Danuar Sanctuary", "DS", "Zuflucht der Ruhn", "ZDR"});
	addLocation(WorldMapType::SEIZED_DANUAR_SANCTUARY, 388.66f, 1185.07f, 55.31f, {"Seized Danuar Sanctuary", "SDS", "Verlorene Zukunft", "VZ"});
	addLocation(WorldMapType::THE_NIGHTMARE_CIRCUS, 467.64f, 568.34f, 201.67f, {"Nightmare Circus", "NC", "Rukibuki Zirkus"});
	addLocation(WorldMapType::KAMAR_BATTLEFIELD, 1329, 1501, 593, {"Kamar Battlefield", "KB", "Schlachtfeld von Kamar"});
	// 4.3 Instance
	addLocation(WorldMapType::HEXWAY, 672.8f, 606.2f, 321.2f, {"The Hexway", "Korridor des Verrats", "KDV"});
	// 4.5 instances
	addLocation(WorldMapType::ENGULFED_OPHIDAN_BRIDGE, 753, 532, 577, {"Engulfed Ophidian Bridge", "EOB", "Jormungand Marschroute"});
	addLocation(WorldMapType::IRON_WALL_WARFRONT, 550, 477, 213, {"Iron Wall Warfront", "IWW", "Schlachtfeld der Stahlmauerbastion"});
	addLocation(WorldMapType::LUCKY_OPHIDAN_BRIDGE, 750, 554, 574, {"Lucky Ophidian Bridge", "LOB", "Lucky Ophidan", "Jormungand Bonus"});
	addLocation(WorldMapType::LUCKY_DANUAR_RELIQUARY, 750, 554, 574, {"Lucky Danuar Reliquary", "LDR", "Lucky Danuar"});
	addLocation(WorldMapType::ILLUMINARY_OBELISK, 322.38f, 324.47f, 405.49997f, {"Illuminary Obelisk", "IO", "Schutzturm"});
	// Quest Instance Maps
	addLocation(WorldMapType::KARAMATIS, 221, 250, 206, {"Karamatis 1"});
	addLocation(WorldMapType::KARAMATIS_B, 312, 274, 206, {"Karamatis 2"});
	addLocation(WorldMapType::IDAB_PRO_L3, 221, 250, 206, {"Karamatis 3"});
	addLocation(WorldMapType::AERDINA, 275, 168, 205, {"Aerdina"});
	addLocation(WorldMapType::GERANAIA, 275, 168, 205, {"Geranaia"});
	// Stigma quest
	addLocation(WorldMapType::IDLF1B_STIGMA, 247, 249, 1392, {"Sliver of Darkness", "Fragment der Finsternis"});
	addLocation(WorldMapType::SPACE_OF_DESTINY, 246, 246, 125, {"Space of Destiny", "Raum des Schicksals"});
	addLocation(WorldMapType::ATAXIAR, 221, 250, 206, {"Ataxiar 1"});
	addLocation(WorldMapType::ATAXIAR_B, 221, 250, 206, {"Ataxiar 2"});
	addLocation(WorldMapType::BREGIRUN, 275, 168, 205, {"Bregirun"});
	addLocation(WorldMapType::NIDALBER, 275, 168, 205, {"Nidalber"});
	// Arenas
	addLocation(WorldMapType::SANCTUM_UNDERGROUND_ARENA, 275, 242, 159, {"Sanctum Arena", "Unterirdische Arena von Sanctum"});
	addLocation(WorldMapType::TRINIEL_UNDERGROUND_ARENA, 275, 239, 159, {"Triniel Arena", "Triniels unterirdische Arena"});
	// Empyrean Crucible
	addLocation(WorldMapType::EMPYREAN_CRUCIBLE, 380, 350, 95, {"Crucible 1-0"});
	addLocation(WorldMapType::EMPYREAN_CRUCIBLE, 346, 350, 96, {"Crucible 1-1"});
	addLocation(WorldMapType::EMPYREAN_CRUCIBLE, 1265, 821, 359, {"Crucible 5-0"});
	addLocation(WorldMapType::EMPYREAN_CRUCIBLE, 1256, 797, 359, {"Crucible 5-1"});
	addLocation(WorldMapType::EMPYREAN_CRUCIBLE, 1596, 150, 129, {"Crucible 6-0"});
	addLocation(WorldMapType::EMPYREAN_CRUCIBLE, 1628, 155, 126, {"Crucible 6-1"});
	addLocation(WorldMapType::EMPYREAN_CRUCIBLE, 1813, 797, 470, {"Crucible 7-0"});
	addLocation(WorldMapType::EMPYREAN_CRUCIBLE, 1785, 797, 470, {"Crucible 7-1"});
	addLocation(WorldMapType::EMPYREAN_CRUCIBLE, 1776, 1728, 304, {"Crucible 8-0"});
	addLocation(WorldMapType::EMPYREAN_CRUCIBLE, 1776, 1760, 304, {"Crucible 8-1"});
	addLocation(WorldMapType::EMPYREAN_CRUCIBLE, 1357, 1748, 320, {"Crucible 9-0"});
	addLocation(WorldMapType::EMPYREAN_CRUCIBLE, 1334, 1741, 316, {"Crucible 9-1"});
	addLocation(WorldMapType::EMPYREAN_CRUCIBLE, 1750, 1255, 395, {"Crucible 10-0"});
	addLocation(WorldMapType::EMPYREAN_CRUCIBLE, 1761, 1280, 395, {"Crucible 10-1"});
	// Arena Of Chaos
	addLocation(WorldMapType::ARENA_OF_CHAOS, 1332, 1078, 340, {"Arena of Chaos 1"});
	addLocation(WorldMapType::ARENA_OF_CHAOS, 599, 1854, 227, {"Arena of Chaos 2"});
	addLocation(WorldMapType::ARENA_OF_CHAOS, 663, 265, 512, {"Arena of Chaos 3"});
	addLocation(WorldMapType::ARENA_OF_CHAOS, 1840, 1730, 302, {"Arena of Chaos 4"});
	addLocation(WorldMapType::ARENA_OF_CHAOS, 1932, 1228, 270, {"Arena of Chaos 5"});
	addLocation(WorldMapType::ARENA_OF_CHAOS, 1949, 946, 224, {"Arena of Chaos 6"});
	// Miscellaneous
	// Prison
	addLocation(WorldMapType::LF_PRISON, 256, 256, 49, {"Prison LF", "Prison Elyos"});
	addLocation(WorldMapType::DF_PRISON, 256, 256, 49, {"Prison DF", "Prison Asmos"});
	// Test
	addLocation(WorldMapType::ID_TEST_DUNGEON, 104, 66, 25, {"Test Dungeon"});
	addLocation(WorldMapType::TEST_BASIC, 144, 136, 20, {"Test Basic"});
	addLocation(WorldMapType::TEST_SERVER, 228, 171, 49, {"Test Server"});
	addLocation(WorldMapType::TEST_GIANTMONSTER, 196, 187, 20, {"Test Giantmonster"});
	// Unknown
	addLocation(WorldMapType::NO_ZONE_NAME, 270, 200, 206, {"IDAbPro"});
	// GM zone
	addLocation(WorldMapType::HOUSING_BARRACK, 2594.29f, 85.48f, 121.0f, int8_t{20}, {"GM"});
	// 2.5 Maps
	addLocation(WorldMapType::KAISINEL_ACADEMY, 459, 251, 128, {"Kaisinel Academy", "Kaisinels Akademie"});
	addLocation(WorldMapType::MARCHUTAN_PRIORY, 577, 250, 94, {"Marchutan Priory", "Marchutans Konzil"});
	addLocation(WorldMapType::ESOTERRACE, 333, 437, 326, {"Esoterrace", "Esoterrasse"});
	// 3.0 Maps
	addLocation(WorldMapType::PERNON, 1069, 1539, 98, {"Pernon"});
	addLocation(WorldMapType::ORIEL, 1261, 1845, 98, {"Oriel", "Elian"});
	addLocation(WorldMapType::RENTUS_BASE, 557, 593, 154, {"Rentus", "Rentus Base", "Rentus Basis"});
	// 3.5
	addLocation(WorldMapType::TIAMAT_STRONGHOLD, 1581, 1068, 492, {"Tiamat Stronghold", "Tiamats Festung", "TF"});
	addLocation(WorldMapType::DRAGON_LORDS_REFUGE, 506, 516, 242, {"Dragon Lords Refuge", "Tiamats Unterschlupf", "TU"});
	addLocation(WorldMapType::DRAGON_LORDS_REFUGE, 495, 528, 417, {"Throne of Blood", "Tiamat", "Blutthron"});
	// 4.0 Instances
	addLocation(WorldMapType::INFINITY_SHARD, 109, 131, 125, {"Infinity Shard", "Katalamize"});
	// 4.7 Instances
	addLocation(WorldMapType::IDGEL_DOME, 254, 179, 83, {"Idgel Dome", "Ruhnatorium"});
	addLocation(WorldMapType::LINKGATE_FOUNDRY, 362, 260, 312, {"Linkgate Foundry", "Baruna Forschungslabor"});
	addLocation(WorldMapType::INFERNAL_ILLUMINARY_OBELISK, 322.38f, 324.47f, 405.49997f, {"Infernal Illuminary Obelisk", "IIO", "Schutzturm Heroisch"});
	addLocation(WorldMapType::THE_SHUGO_EMPERORS_VAULT, 542.9366f, 299.9885f, 401.0f, int8_t{22}, {"Shugo Emperors Vault"});
	// New map 4.7
	addLocation(WorldMapType::KALDOR, 397, 1380, 163, {"Kaldor"});
	addLocation(WorldMapType::LEVINSHOR, 207, 183, 374, {"Levinshor", "Akaron"});
	addLocation(WorldMapType::BELUS, 1238, 1232, 1518, {"Belus"});
	addLocation(WorldMapType::TRANSIDIUM_ANNEX, 509, 513, 675, {"Transidium Annex", "Antriksha", "Ahserion"});
	addLocation(WorldMapType::ASPIDA, 1238, 1232, 1518, {"Aspida"});
	addLocation(WorldMapType::ATANATOS, 1238, 1232, 1518, {"Atanatos", "Athanos"});
	addLocation(WorldMapType::DISILLON, 1238, 1232, 1518, {"Disillon", "Deylon"});
	// New instances 4.8
	addLocation(WorldMapType::OCCUPIED_RENTUS_BASE, 557, 593, 154, {"Occupied Rentus Base", "Occupied Rentus", "Verlorene Rentus Basis"});
	addLocation(WorldMapType::ANGUISHED_DRAGON_LORDS_REFUGE, 506, 516, 242, {"Anguished Dragon Lords Refuge", "Tiamats Hideout HM", "TU HM"});
	addLocation(WorldMapType::DRAKENSPIRE_DEPHTS, 322, 183, 1688, {"Drakenspire Depths", "Makarna"});
	addLocation(WorldMapType::RAKSANG_RUINS, 830, 942, 1207, int8_t{73}, {"Raksang Ruins", "Mantor"});
	addLocation(WorldMapType::INFERNAL_DANUAR_RELIQUARY, 256.60f, 257.99f, 241.78f, {"Infernal Danuar Reliquary", "IDR", "Ruhnadium Heroisch"});
	addLocation(WorldMapType::STONESPEAR_REACH, 231.14f, 264.399f, 98, {"Stonespear Reach", "Plaza of Challenge", "Platz der Herausforderung"});
	// New maps 4.8
	addLocation(WorldMapType::CYGNEA, 2905, 803, 570, {"Cygnea", "Signia"});
	addLocation(WorldMapType::CYGNEA, 1360, 613, 583, {"Kenoa"});
	addLocation(WorldMapType::CYGNEA, 1186, 1610, 468, {"Deluan"});
	addLocation(WorldMapType::CYGNEA, 2368, 1507, 440, {"Attika"});
	addLocation(WorldMapType::CYGNEA, 2134, 2912, 329, {"Außenposten der Legion des Neubeginns", "Außenposten Ldn"});
	addLocation(WorldMapType::CYGNEA, 518, 1900, 469, {"Angriffsposten der Legion des Neubeginns", "Angriffsposten Ldn"});
	addLocation(WorldMapType::ENSHAR, 454, 2262, 220, {"Enshar", "Vengar"});
	addLocation(WorldMapType::ENSHAR, 1768, 2555, 300, {"Mura"});
	addLocation(WorldMapType::ENSHAR, 1475, 1746, 330, {"Satyr"});
	addLocation(WorldMapType::ENSHAR, 759, 1274, 253, {"Velias"});
	addLocation(WorldMapType::ENSHAR, 1578, 143, 188, {"Oasentempel"});
	addLocation(WorldMapType::ENSHAR, 2685, 1407, 341, {"Tempel des Sonnenuntergangs"});
	addLocation(WorldMapType::GRIFFOEN, 263, 127, 501, {"Griffoen", "Ellegef"});
	addLocation(WorldMapType::HABROK, 253, 107, 505, {"Habrok", "Lagnatun"});
	addLocation(WorldMapType::IDIAN_DEPTHS_LIGHT, 684, 654, 515, {"Idian Depths Elyos", "Untergrund von Katalam Elyos", "UGE"});
	addLocation(WorldMapType::IDIAN_DEPTHS_DARK, 684, 654, 515, {"Idian Depths Asmo", "Untergrund von Katalam Asmo", "UGA"});
}

// Java GoTo.java:412-414
void GoTo::addLocation(WorldMapType mapType, float x, float y, float z, std::initializer_list<std::string> identifiers) {
	addLocation(mapType, x, y, z, int8_t{0}, identifiers); // parity= addLocation(mapType, x, y, z, (byte) 0, identifiers);
}

// Java GoTo.java:416-422
void GoTo::addLocation(WorldMapType mapType, float x, float y, float z, int8_t h, std::initializer_list<std::string> identifiers) {
	runtime::Ref<Location> location = Location::create(mapType, x, y, z, h, identifiers); // parity= Location location = new Location(mapType, x, y, z, h, Arrays.asList(identifiers));
	for (const std::string& identifier : identifiers) {
		if (locations.put(commons::utils::StringUtils::toLowerCase(identifier), location) != nullptr) // parity= if (locations.put(identifier.toLowerCase(), location) != null)
			throw runtime::IllegalArgumentException("Duplicate location identifier: " + identifier);
	}
}

// Java GoTo.java:431-438
GoTo::Location::Location(WorldMapType mapType_, float x_, float y_, float z_, int8_t h_, std::initializer_list<std::string> identifiers_)
	: mapType(mapType_), x(x_), y(y_), z(z_), h(h_) { // parity= this.mapType = mapType; this.x = x; this.y = y; this.z = z; this.h = h;
	for (const std::string& identifier : identifiers_) // parity= this.identifiers = identifiers;
		identifiers.add(identifier); // parity: (the same)
}

runtime::Ref<GoTo::Location> GoTo::Location::create(WorldMapType mapType, float x, float y, float z, int8_t h, // parity: C++ factory
	std::initializer_list<std::string> identifiers) { // parity: (the same)
	return runtime::makeRef<Location>(mapType, x, y, z, h, identifiers); // parity: (the same)
}

GoTo::Location::~Location() = default; // parity: C++ destructor

} // namespace aion::gameserver::handlers::admincommands
