#include "aion/gameserver/handlers/admincommands/MoveTo.h"

#include <cmath>
#include <optional>
#include <regex>

#include "aion/commons/configuration/transformers/NumberTransformer.h"
#include "aion/commons/utils/Numbers.h"
#include "aion/commons/utils/StringUtils.h"
#include "aion/gameserver/configs/main/GeoDataConfig.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/NpcData.h"
#include "aion/gameserver/dataholders/SpawnsData.h"
#include "aion/gameserver/geoEngine/math/JavaFloat.h"
#include "aion/gameserver/model/templates/npc/NpcTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_POSITION.h"
#include "aion/gameserver/services/instance/InstanceService.h"
#include "aion/gameserver/services/teleport/TeleportService.h"
#include "aion/gameserver/utils/ChatUtil.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/utils/PositionUtil.h"
#include "aion/gameserver/utils/Util.h"
#include "aion/gameserver/world/World.h"
#include "aion/gameserver/world/WorldMap.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldMapTypeInfo.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::handlers::admincommands {

AION_ADMIN_COMMAND(MoveTo);

// Java MoveTo.java:25-35
MoveTo::MoveTo()
	: AdminCommand("moveto", "Moves you to any location.",
		  "<x> <y> [z] - Moves you to the specified coordinates on the current map (also supports pasted xml attributes like x=\"1422.7744\" "
		  "y=\"1250.0612\" z=\"569.47\").\n"
		  "<map name|ID> <x> <y> [z] - Moves you to the specified position (map names need underscores instead of spaces).\n"
		  "<position link> - Moves you to the position of the chat link.\n"
		  "<player name> - Moves you to the player.\n"
		  "<npc name|ID> - Moves you to a spawn spot of the NPC.\n"
		  "forward <distance> - Moves you forward by the specified distance, ignoring any obstacles in between.\n") {
}

// Java MoveTo.java:37-82
void MoveTo::execute(Player& admin, std::span<const std::string> params) {
	using geoEngine::math::JavaFloat; // parity: C++ using-declaration
	if (params.size() < 1) {
		sendInfo(admin);
		return;
	}
	std::optional<std::string> errorMsg; // parity= String errorMsg = null;
	if (params.size() == 2 && commons::utils::StringUtils::equalsIgnoreCase("forward", params[0])) { // parity= if (params.length == 2 && "forward".equalsIgnoreCase(params[0])) {
		moveForward(admin, commons::configuration::transformers::NumberParser::parseFloat(params[1])); // parity= moveForward(admin, Float.parseFloat(params[1]));
		return;
	}
	runtime::Ref<WorldPosition> pos = params.size() == 1 ? ChatUtil::getPosition(params[0]) : parseWorldPosition(admin, params); // parity= WorldPosition pos = params.length == 1 ? ChatUtil.getPosition(params[0]) : parseWorldPosition(admin, params);
	if (pos != nullptr) {
		pos->setH(admin.getHeading());
		moveTo(admin, pos, // parity= moveTo(admin, pos, "Teleported to " + worldName(pos.getMapId()) + "\nX:" + pos.getX() + " Y:" + pos.getY() + " Z:" + pos.getZ());
			"Teleported to " + worldName(pos->getMapId()) + "\nX:" + JavaFloat::toString(pos->getX()) + " Y:" + JavaFloat::toString(pos->getY()) + // parity: (the same)
				" Z:" + JavaFloat::toString(pos->getZ())); // parity: (the same)
		return;
	} else if (params.size() > 1 || params[0].starts_with("[pos:")) // parity= } else if (params.length > 1 || params[0].startsWith("[pos:"))
		errorMsg = std::string("Invalid map position or ") + (GeoDataConfig::GEO_ENABLE.load() ? "missing" : "deactivated") + " geo."; // parity= errorMsg = "Invalid map position or %s geo.".formatted(GeoDataConfig.GEO_ENABLE ? "missing" : "deactivated");

	std::string nameOrId = commons::utils::StringUtils::toLowerCase(join(params, 0)); // parity= String nameOrId = String.join(" ", params).toLowerCase();
	runtime::Ptr<Player> player = World::getInstance().getPlayer(Util::convertName(nameOrId));
	if (player != nullptr && !player->equals(admin)) {
		moveTo(admin, runtime::Ref<WorldPosition>(player->getPosition()), "Teleported to " + name(*player) + "."); // parity= moveTo(admin, player.getPosition(), "Teleported to " + name(player) + ".");
		return;
	} else if (!errorMsg || (player != nullptr && admin.equals(*player))) { // parity= } else if (errorMsg == null || admin.equals(player)) {
		errorMsg = "Invalid player name or player is offline.";
	}

	int32_t npcId = getNpcId(nameOrId);
	if (npcId > 0 && DataManager::SPAWNS_DATA->getFirstSpawnByNpcId(0, npcId)) { // parity= if (npcId > 0 && DataManager.SPAWNS_DATA.getFirstSpawnByNpcId(0, npcId) != null) {
		sendInfo(admin, "Teleported to " + ChatUtil::path(npcId, true) + ".");
		TeleportService::teleportToNpc(admin, npcId);
		return;
	} else if (npcId > 0) {
		errorMsg = "Could not find " + ChatUtil::path(npcId, true) + ".";
	} else if (nameOrId.find(' ') != std::string::npos) { // parity= } else if (nameOrId.contains(" ")) {
		errorMsg = "Could not find \"" + nameOrId + "\".";
	}

	sendInfo(admin, *errorMsg); // parity= sendInfo(admin, errorMsg);
}

// Java MoveTo.java:84-90
void MoveTo::moveForward(Player& admin, float distance) {
	double radian = PositionUtil::convertHeadingToAngle(admin.getHeading()) * 0.017453292519943295; // parity= double radian = Math.toRadians(PositionUtil.convertHeadingToAngle(admin.getHeading()));
	float newX = static_cast<float>(admin.getX() + std::cos(radian) * distance);
	float newY = static_cast<float>(admin.getY() + std::sin(radian) * distance);
	admin.getPosition()->setXYZH(newX, newY, admin.getZ(), admin.getHeading());
	PacketSendUtility::broadcastToSightedPlayers(admin, SM_POSITION(admin), true);
}

// Java MoveTo.java:92-123
runtime::Ref<WorldPosition> MoveTo::parseWorldPosition(Player& admin, std::span<const std::string> params) {
	size_t coordIndex = 0;
	int32_t mapId;
	static const auto* const mapIdPattern = new std::regex("[1-9][0-9]{8,}"); // parity: (the pattern of String.matches, below)
	static const auto* const mapNamePattern = new std::regex("[a-zA-Z_]+"); // parity: (the same)
	bool isMapId = std::regex_match(params[0], *mapIdPattern); // parity= boolean isMapId = params[0].matches("[1-9][0-9]{8,}");
	bool isMapName = std::regex_match(params[0], *mapNamePattern); // parity= boolean isMapName = params[0].matches("[a-zA-Z_]+");
	if (isMapId || isMapName) {
		mapId = isMapId ? commons::utils::parseInt(params[0]) : world::getWorldMapTypeMapId(params[0]); // parity= mapId = isMapId ? Integer.parseInt(params[0]) : WorldMapType.getMapId(params[0]);
		coordIndex = 1;
	} else {
		mapId = admin.getWorldId() + admin.getInstanceId() - 1;
	}
	std::optional<float> x, y, z; // parity= Float x = null, y = null, z = null;
	// Java: (?<type>x|y|z) is group 2, (?<coord>...) group 4 (std::regex has no named groups)
	static const auto* const p = new std::regex("^((x|y|z)(=|:)\"?)?([0-9]+(\\.[0-9]+)?f?)\"?,?$", std::regex::icase); // parity= Pattern p = Pattern.compile("^((?<type>x|y|z)(=|:)\"?)?(?<coord>[0-9]+(\\.[0-9]+)?f?)\"?,?$", Pattern.CASE_INSENSITIVE);
	size_t maxIndex = std::min(params.size(), coordIndex + 3);
	for (size_t i = coordIndex; i < maxIndex; i++) {
		std::smatch m; // parity= Matcher m = p.matcher(params[i]);
		if (std::regex_search(params[i], m, *p)) { // parity= if (m.find()) {
			float coord = commons::configuration::transformers::NumberParser::parseFloat(m[4].str()); // parity= float coord = Float.parseFloat(m.group("coord"));
			std::optional<std::string> type = m[2].matched ? std::optional<std::string>(m[2].str()) : std::nullopt; // parity= String type = m.group("type");
			if ((type && commons::utils::StringUtils::equalsIgnoreCase("x", *type)) || (!x && !type)) // parity= if ("x".equalsIgnoreCase(type) || x == null && type == null)
				x = coord;
			else if ((type && commons::utils::StringUtils::equalsIgnoreCase("y", *type)) || (!y && !type)) // parity= else if ("y".equalsIgnoreCase(type) || y == null && type == null)
				y = coord;
			else if ((type && commons::utils::StringUtils::equalsIgnoreCase("z", *type)) || (!z && !type)) // parity= else if ("z".equalsIgnoreCase(type) || z == null && type == null)
				z = coord;
		} else {
			return nullptr;
		}
	}
	return !x || !y ? nullptr : ChatUtil::parsedCoordsToWorldPosition(mapId, *x, *y, z, std::nullopt); // parity= return x == null || y == null ? null : ChatUtil.parsedCoordsToWorldPosition(mapId, x, y, z, null);
}

// Java MoveTo.java:125-133
void MoveTo::moveTo(Player& admin, runtime::Ref<WorldPosition> pos, const std::string& message) {
	sendInfo(admin, message); // msg before teleport, otherwise client could ignore it
	if (pos->isInstanceMap() && pos->getWorldMapInstance()->getParent()->getMainWorldMapInstance() == pos->getWorldMapInstance()) {
		// currently instance type maps have a main world map instance (default) which have no spawns, so we create a new instance instead
		runtime::Ptr<WorldMapInstance> instance = InstanceService::getOrRegisterInstance(pos->getMapId(), admin);
		pos = World::getInstance().createPosition(pos->getMapId(), pos->getX(), pos->getY(), pos->getZ(), pos->getHeading(), instance->getInstanceId());
	}
	TeleportService::teleportTo(admin, *pos);
}

// Java MoveTo.java:135-143
int32_t MoveTo::getNpcId(const std::string& nameOrId) {
	static const auto* const idPattern = new std::regex("[1-9][0-9]{5}"); // parity: (the pattern of String.matches, below)
	if (std::regex_match(nameOrId, *idPattern)) // parity= if (nameOrId.matches("[1-9][0-9]{5}"))
		return commons::utils::parseInt(nameOrId);
	for (const NpcTemplate* template_ : DataManager::NPC_DATA->getNpcData()) {
		if (commons::utils::StringUtils::toLowerCase(template_->getName()) == nameOrId) // parity= if (template.getName().toLowerCase().equals(nameOrId))
			return template_->getTemplateId();
	}
	return 0;
}

} // namespace aion::gameserver::handlers::admincommands
