#include "aion/gameserver/handlers/admincommands/BaseCommand.h"

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

#include "aion/commons/utils/Numbers.h"
#include "aion/commons/utils/StringUtils.h"
#include "aion/gameserver/dataholders/loadingutils/EnumTraits.h"
#include "aion/gameserver/model/base/Base.h"
#include "aion/gameserver/model/base/BaseLocation.h"
#include "aion/gameserver/model/base/BaseOccupier.h"
#include "aion/gameserver/services/BaseService.h"
#include "aion/gameserver/spawnengine/SpawnHandlerType.h"
#include "aion/gameserver/utils/EnumValueOf.h"

namespace aion::gameserver::handlers::admincommands {

AION_ADMIN_COMMAND(BaseCommand);

BaseCommand::BaseCommand()
	: AdminCommand("base", "Lists bases or changes their state.", R"(list - Lists all available base locations with their respective occupier.
start <id> - Activates the specified base.
stop <id> - Deactivates the specified base.
capture <id> <occupier> - Captures the specified base with the specified new occupier.
assault <id> - Spawns attacker NPCs for the specified base if available.
)") {
}

// Java BaseCommand.java:30-45
void BaseCommand::execute(Player& player, std::span<const std::string> params) {
	if (params.empty()) {
		sendInfo(player);
		return;
	}

	const std::string command = commons::utils::StringUtils::toLowerCase(params[0]);
	if (command == "list")
		showBaseLocationList(player);
	else if (command == "start")
		startBase(player, params);
	else if (command == "stop")
		stopBase(player, params);
	else if (command == "capture")
		captureBase(player, params);
	else if (command == "assault")
		assaultBase(player, params);
	else
		sendInfo(player);
}

// Java BaseCommand.java:47-51
void BaseCommand::showBaseLocationList(Player& player) {
	std::vector<runtime::Ptr<BaseLocation>> locations = BaseService::getInstance().getBaseLocations();
	std::stable_sort(locations.begin(), locations.end(), [](const runtime::Ptr<BaseLocation>& a, const runtime::Ptr<BaseLocation>& b) {
		return a->getId() < b->getId();
	});
	for (const runtime::Ptr<BaseLocation>& loc : locations)
		sendInfo(player, "Base " + std::to_string(loc->getId()) + " belongs to " + std::string(xml::enumName(loc->getOccupier())));
}

// Java BaseCommand.java:53-63
void BaseCommand::startBase(Player& player, std::span<const std::string> params) {
	const int32_t baseId = parseBaseId(player, params, 2);
	if (baseId == 0)
		return;

	if (BaseService::getInstance().isActive(baseId)) {
		sendInfo(player, "Base is already active");
		return;
	}
	BaseService::getInstance().start(baseId);
}

// Java BaseCommand.java:65-75
void BaseCommand::stopBase(Player& player, std::span<const std::string> params) {
	const int32_t baseId = parseBaseId(player, params, 2);
	if (baseId == 0)
		return;

	if (!BaseService::getInstance().isActive(baseId)) {
		sendInfo(player, "Base is already inactive.");
		return;
	}
	BaseService::getInstance().stop(baseId);
}

// Java BaseCommand.java:77-89
void BaseCommand::captureBase(Player& player, std::span<const std::string> params) {
	const int32_t baseId = parseBaseId(player, params, 3);
	if (baseId == 0)
		return;

	if (!BaseService::getInstance().isActive(baseId)) {
		sendInfo(player, "Inactive bases cannot be captured.");
		return;
	}

	const BaseOccupier occupier = utils::enumValueOf<BaseOccupier>(commons::utils::StringUtils::toUpperCase(params[2]));
	BaseService::getInstance().capture(baseId, occupier);
}

// Java BaseCommand.java:91-110
void BaseCommand::assaultBase(Player& player, std::span<const std::string> params) {
	const int32_t baseId = parseBaseId(player, params, 3);
	if (baseId == 0)
		return;

	runtime::Ptr<Base> base = BaseService::getInstance().getActiveBase(baseId);
	if (!base) {
		sendInfo(player, "Inactive bases cannot be assaulted.");
		return;
	}
	if (base->isUnderAssault()) {
		sendInfo(player, "Base is already under assault.");
		return;
	}
	const BaseOccupier occupier = utils::enumValueOf<BaseOccupier>(commons::utils::StringUtils::toUpperCase(params[2]));
	if (base->getOccupier() == occupier) {
		sendInfo(player, "Base cannot be assaulted by the same occupier");
		return;
	}
	base->spawnBySpawnHandler(spawnengine::SpawnHandlerType::ATTACKER, occupier);
}

// Java BaseCommand.java:112-125
int32_t BaseCommand::parseBaseId(Player& admin, std::span<const std::string> params, int32_t expectedParameters) {
	if (static_cast<int32_t>(params.size()) < expectedParameters) {
		sendInfo(admin, "Not enough parameters.");
		return 0;
	}

	const int32_t baseId = commons::utils::parseInt(params[1]);
	if (!BaseService::getInstance().getBaseLocation(baseId)) {
		sendInfo(admin, "Invalid base ID.");
		return 0;
	}

	return baseId;
}

} // namespace aion::gameserver::handlers::admincommands
