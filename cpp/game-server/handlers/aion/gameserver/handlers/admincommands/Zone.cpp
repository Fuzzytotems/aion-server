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
	std::optional<std::string_view> zoneNameParam =
		params.empty() ? std::nullopt : std::optional<std::string_view>(params[0]);
	std::vector<runtime::Ptr<ZoneInstance>> zones = findZones(target, zoneNameParam);
	// Java: Arrays.stream(ZoneType.values()).filter(target::isInsideZoneType).map(ZoneType::name).collect(Collectors.joining(", "))
	std::string zoneTypes;
	const auto& zoneTypeNames = xml::EnumTraits<ZoneType>::names;
	for (size_t ordinal = 0; ordinal < zoneTypeNames.size(); ++ordinal) {
		if (target.isInsideZoneType(static_cast<ZoneType>(ordinal))) {
			if (!zoneTypes.empty())
				zoneTypes += ", ";
			zoneTypes += zoneTypeNames[ordinal];
		}
	}
	if (!zoneTypes.empty())
		sendInfo(admin, name(target) + "'s zone types: " + zoneTypes);
	if (zones.empty()) {
		sendInfo(admin, name(target) + " is not in " + (zoneNameParam ? std::string(*zoneNameParam) : std::string("any zone")) + '.');
	} else {
		sendInfo(admin, name(target) + "'s " + (zones.size() == 1 ? "zone" : "zones") + ':');
		auto b = [](bool value) { return std::string(value ? "true" : "false"); }; // Java: String.valueOf(boolean)
		for (const runtime::Ptr<ZoneInstance>& zone : zones) {
			sendInfo(admin, zone->getAreaTemplate()->getZoneName()->name());
			sendInfo(admin, "Fly: " + b(zone->canFly()) + "; Glide: " + b(zone->canGlide()));
			sendInfo(admin, "Ride: " + b(zone->canRide()) + "; Fly-ride: " + b(zone->canFlyRide()));
			sendInfo(admin, "Kisk: " + b(zone->canPutKisk()) + "; Recall: " + b(zone->canRecall()));
			sendInfo(admin, "Same race duels: " + b(zone->isSameRaceDuelsAllowed()) + "; Other race duels: " + b(zone->isOtherRaceDuelsAllowed()));
			sendInfo(admin, "PvP: " + b(zone->isPvpAllowed()));
			sendInfo(admin, "canReturnBattle: " + b(zone->canReturnToBattle()));
		}
	}
}

// Java Zone.java:65-74
std::vector<runtime::Ptr<ZoneInstance>> Zone::findZones(Creature& creature, std::optional<std::string_view> zoneNameFilter) {
	std::vector<runtime::Ptr<ZoneInstance>> zones = creature.findZones();
	if (zoneNameFilter) {
		const ZoneName* zoneName = ZoneName::get(*zoneNameFilter);
		if (zoneName == ZoneName::NONE)
			throw runtime::IllegalArgumentException("Invalid zone name.");
		std::vector<runtime::Ptr<ZoneInstance>> filtered;
		for (const runtime::Ptr<ZoneInstance>& zone : zones) {
			if (zone->getZoneTemplate()->getName() == zoneName)
				filtered.push_back(zone);
		}
		zones = std::move(filtered);
	}
	return zones;
}

} // namespace aion::gameserver::handlers::admincommands
