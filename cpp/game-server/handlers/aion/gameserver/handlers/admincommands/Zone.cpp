#include "aion/gameserver/handlers/admincommands/Zone.h"

#include "aion/commons/utils/StringUtils.h"
#include "aion/gameserver/model/geometry/Area.h"
#include "aion/gameserver/model/templates/zone/ZoneTemplate.h"
#include "aion/gameserver/world/zone/ZoneInstance.h"
#include "aion/gameserver/world/zone/ZoneName.h"

namespace aion::gameserver::handlers::admincommands {

AION_ADMIN_COMMAND(Zone);

Zone::Zone()
	: AdminCommand("zone", "Shows zone information.",
		  " - Shows info about your target's current zone(s).\n"
		  "<zone name> - Shows info about your target's current zone(s), filtered by the given zone name.\n"
		  "refresh - Refreshes your zones.\n") {
}

// Java Zone.java:30-63
void Zone::execute(Player& admin, std::span<const std::string> params) {
	if (params.size() > 1) {
		sendInfo(admin);
		return;
	}
	if (params.size() == 1 && commons::utils::StringUtils::equalsIgnoreCase("refresh", params[0])) {
		admin.revalidateZones();
		return;
	}
	runtime::Ptr<Creature> targetCreature = runtime::as<Creature>(admin.getTarget());
	Creature& target = targetCreature != nullptr ? *targetCreature : static_cast<Creature&>(admin);
	std::optional<std::string_view> zoneNameParam = // parity= String zoneNameParam = params.length == 0 ? null : params[0];
		params.empty() ? std::nullopt : std::optional<std::string_view>(params[0]); // parity: (continued)
	std::vector<runtime::Ptr<ZoneInstance>> zones = findZones(target, zoneNameParam);
	// Java: Arrays.stream(ZoneType.values()).filter(target::isInsideZoneType).map(ZoneType::name).collect(Collectors.joining(", "))
	std::string zoneTypes; // parity= String zoneTypes = Arrays.stream(ZoneType.values()).filter(target::isInsideZoneType).map(ZoneType::name).collect(Collectors.joining(", "));
	const auto& zoneTypeNames = xml::EnumTraits<ZoneType>::names; // parity: (continued)
	for (size_t ordinal = 0; ordinal < zoneTypeNames.size(); ++ordinal) { // parity: (continued)
		if (target.isInsideZoneType(static_cast<ZoneType>(ordinal))) { // parity: (continued)
			if (!zoneTypes.empty()) // parity: (continued)
				zoneTypes += ", "; // parity: (continued)
			zoneTypes += zoneTypeNames[ordinal]; // parity: (continued)
		}
	}
	if (!zoneTypes.empty())
		sendInfo(admin, name(target) + "'s zone types: " + zoneTypes);
	if (zones.empty()) {
		sendInfo(admin, name(target) + " is not in " + (zoneNameParam ? std::string(*zoneNameParam) : std::string("any zone")) + '.'); // parity= sendInfo(admin, name(target) + " is not in " + (zoneNameParam == null ? "any zone" : zoneNameParam) + '.');
	} else {
		sendInfo(admin, name(target) + "'s " + (zones.size() == 1 ? "zone" : "zones") + ':');
		auto b = [](bool value) { return std::string(value ? "true" : "false"); }; // parity: Java's String.valueOf(boolean) in a concatenation
		for (const runtime::Ptr<ZoneInstance>& zone : zones) {
			sendInfo(admin, zone->getAreaTemplate()->getZoneName()->name());
			sendInfo(admin, "Fly: " + b(zone->canFly()) + "; Glide: " + b(zone->canGlide())); // parity= sendInfo(admin, "Fly: " + zone.canFly() + "; Glide: " + zone.canGlide());
			sendInfo(admin, "Ride: " + b(zone->canRide()) + "; Fly-ride: " + b(zone->canFlyRide())); // parity= sendInfo(admin, "Ride: " + zone.canRide() + "; Fly-ride: " + zone.canFlyRide());
			sendInfo(admin, "Kisk: " + b(zone->canPutKisk()) + "; Recall: " + b(zone->canRecall())); // parity= sendInfo(admin, "Kisk: " + zone.canPutKisk() + "; Recall: " + zone.canRecall());
			sendInfo(admin, "Same race duels: " + b(zone->isSameRaceDuelsAllowed()) + "; Other race duels: " + b(zone->isOtherRaceDuelsAllowed())); // parity= sendInfo(admin, "Same race duels: " + zone.isSameRaceDuelsAllowed() + "; Other race duels: " + zone.isOtherRaceDuelsAllowed());
			sendInfo(admin, "PvP: " + b(zone->isPvpAllowed())); // parity= sendInfo(admin, "PvP: " + zone.isPvpAllowed());
			sendInfo(admin, "canReturnBattle: " + b(zone->canReturnToBattle())); // parity= sendInfo(admin, "canReturnBattle: " + zone.canReturnToBattle());
		}
	}
}

// Java Zone.java:65-74
std::vector<runtime::Ptr<ZoneInstance>> Zone::findZones(Creature& creature, std::optional<std::string_view> zoneNameFilter) {
	std::vector<runtime::Ptr<ZoneInstance>> zones = creature.findZones();
	if (zoneNameFilter) { // parity= if (zoneNameFilter != null) {
		const ZoneName* zoneName = ZoneName::get(*zoneNameFilter);
		if (zoneName == ZoneName::NONE)
			throw runtime::IllegalArgumentException("Invalid zone name.");
		std::vector<runtime::Ptr<ZoneInstance>> filtered; // parity= zones = zones.stream().filter(zone -> zone.getZoneTemplate().getName() == zoneName).toList();
		for (const runtime::Ptr<ZoneInstance>& zone : zones) { // parity: (continued)
			if (zone->getZoneTemplate()->getName() == zoneName) // parity: (continued)
				filtered.push_back(zone); // parity: (continued)
		} // parity: (continued)
		zones = std::move(filtered); // parity: (continued)
	}
	return zones;
}

} // namespace aion::gameserver::handlers::admincommands
