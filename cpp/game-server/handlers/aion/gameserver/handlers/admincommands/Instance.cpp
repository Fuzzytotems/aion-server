#include "aion/gameserver/handlers/admincommands/Instance.h"

#include <optional>

#include "aion/commons/utils/StringUtils.h"
#include "aion/gameserver/configs/main/AutoGroupConfig.h"
#include "aion/gameserver/dataholders/loadingutils/EnumTraits.h"
#include "aion/gameserver/model/autogroup/AutoGroupTypeInfo.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/services/instance/PeriodicInstanceManager.h"

namespace aion::gameserver::handlers::admincommands {

AION_ADMIN_COMMAND(Instance);

// Java Instance.java:16-24
Instance::Instance()
	: AdminCommand("instance", "Activates or deactivates registration for pvp instances.",
		  "<open|close> dredgion - Opens/closes the registration for Dredgion (6vs6)\n"
		  "<open|close> id - Opens/closes the registration for Idgel Dome (6vs6)\n"
		  "<open|close> eob - Opens/closes the registration for Engulfed Ophidan Bridge (6vs6)\n"
		  "<open|close> kb - Opens/closes the registration for Kamar Battlefield (12vs12)\n"
		  "<open|close> iww - Opens/closes the registration for Iron Wall Warfront (24vs24)\n") {
}

// Java Instance.java:26-36
void Instance::execute(Player& player, std::span<const std::string> params) {
	if (params.size() < 2) {
		sendInfo(player);
		return;
	}
	if (commons::utils::StringUtils::equalsIgnoreCase(params[0], "open")) // parity= if (params[0].equalsIgnoreCase("open"))
		openRegistration(player, params[1]);
	else if (commons::utils::StringUtils::equalsIgnoreCase(params[0], "close")) // parity= else if (params[0].equalsIgnoreCase("close"))
		closeRegistration(player, params[1]);
}

// Java Instance.java:38-83
void Instance::openRegistration(Player& admin, const std::string& instanceName) {
	using configs::main::AutoGroupConfig; // parity: (the static import of AutoGroupConfig.*)
	std::optional<SM_SYSTEM_MESSAGE> openingMsg; // parity= SM_SYSTEM_MESSAGE openingMsg;
	int32_t maskId;
	int64_t registrationPeriod;

	std::string lowerName = commons::utils::StringUtils::toLowerCase(instanceName); // parity= switch (instanceName.toLowerCase()) {
	if (lowerName == "dredgion") { // parity= case "dredgion" -> { // Only opening Terath Dredgion
		openingMsg = SM_SYSTEM_MESSAGE::STR_MSG_INSTANCE_OPEN_IDDREADGION_03();
		maskId = 3;
		registrationPeriod = AutoGroupConfig::DREDGION_REGISTRATION_PERIOD.load(); // parity= registrationPeriod = DREDGION_REGISTRATION_PERIOD;
	} else if (lowerName == "eob") { // parity= case "eob" -> {
		openingMsg = SM_SYSTEM_MESSAGE::STR_MSG_INSTANCE_OPEN_IDLDF5_Under_01_War();
		maskId = 108;
		registrationPeriod = AutoGroupConfig::ENGULFED_OPHIDAN_BRIDGE_REGISTRATION_PERIOD.load(); // parity= registrationPeriod = ENGULFED_OPHIDAN_BRIDGE_REGISTRATION_PERIOD;
	} else if (lowerName == "id") { // parity= case "id" -> {
		openingMsg = SM_SYSTEM_MESSAGE::STR_MSG_INSTANCE_OPEN_IDLDF5_Fortress_Re();
		maskId = 111;
		registrationPeriod = AutoGroupConfig::IDGEL_DOME_REGISTRATION_PERIOD.load(); // parity= registrationPeriod = IDGEL_DOME_REGISTRATION_PERIOD;
	} else if (lowerName == "iww") { // parity= case "iww" -> {
		openingMsg = SM_SYSTEM_MESSAGE::STR_MSG_INSTANCE_OPEN_IDF5_TD_war();
		maskId = 109;
		registrationPeriod = AutoGroupConfig::IRON_WALL_WARFRONT_REGISTRATION_PERIOD.load(); // parity= registrationPeriod = IRON_WALL_WARFRONT_REGISTRATION_PERIOD;
	} else if (lowerName == "kb") { // parity= case "kb" -> {
		openingMsg = SM_SYSTEM_MESSAGE::STR_MSG_INSTANCE_OPEN_IDKamar();
		maskId = 107;
		registrationPeriod = AutoGroupConfig::KAMAR_BATTLEFIELD_REGISTRATION_PERIOD.load(); // parity= registrationPeriod = KAMAR_BATTLEFIELD_REGISTRATION_PERIOD;
	} else { // parity= default -> {
		openingMsg = std::nullopt; // parity= openingMsg = null;
		maskId = 0;
		registrationPeriod = 0;
	}
	if (maskId != 0) {
		if (PeriodicInstanceManager::getInstance().openRegistration(*openingMsg, maskId, registrationPeriod)) // parity= if (PeriodicInstanceManager.getInstance().openRegistration(openingMsg, maskId, registrationPeriod))
			sendInfo(admin, "Registration for " + agtName(maskId) + " is now open."); // parity= sendInfo(admin, "Registration for " + AutoGroupType.getAGTByMaskId(maskId) + " is now open.");
		else
			sendInfo(admin, "Registration for " + agtName(maskId) + " is already open."); // parity= sendInfo(admin, "Registration for " + AutoGroupType.getAGTByMaskId(maskId) + " is already open.");
	} else { // parity= default -> {
		sendInfo(admin, "No instance found for " + instanceName);
	}
}

// Java Instance.java:85-105
void Instance::closeRegistration(Player& admin, const std::string& instanceName) {
	int32_t maskId;

	std::string lowerName = commons::utils::StringUtils::toLowerCase(instanceName); // parity= switch (instanceName.toLowerCase()) {
	if (lowerName == "dredgion") // parity= case "dredgion":
		maskId = 3;
	else if (lowerName == "eob") // parity= case "eob":
		maskId = 108;
	else if (lowerName == "id") // parity= case "id":
		maskId = 111;
	else if (lowerName == "iww") // parity= case "iww":
		maskId = 109;
	else if (lowerName == "kb") // parity= case "kb":
		maskId = 107;
	else // parity= default:
		maskId = 0;

	if (maskId != 0) {
		if (PeriodicInstanceManager::getInstance().closeRegistration(maskId))
			sendInfo(admin, "Registration for " + agtName(maskId) + " is now closed."); // parity= sendInfo(admin, "Registration for " + AutoGroupType.getAGTByMaskId(maskId) + " is now closed.");
		else
			sendInfo(admin, "Registration for " + agtName(maskId) + " is not open."); // parity= sendInfo(admin, "Registration for " + AutoGroupType.getAGTByMaskId(maskId) + " is not open.");
	} else {
		sendInfo(admin, "No instance found for " + instanceName);
	}
}

/** Java: "" + AutoGroupType.getAGTByMaskId(maskId) - the enum constant's name, "null" when there is none */
std::string Instance::agtName(int32_t maskId) { // parity: C++ helper: the string conversion of the enum
	std::optional<AutoGroupType> type = model::autogroup::getAGTByMaskId(maskId); // parity: C++ helper: the string conversion of the enum
	return type ? std::string(xml::enumName(*type)) : std::string("null"); // parity: Java's string conversion of the enum (null: "null")
}

} // namespace aion::gameserver::handlers::admincommands
