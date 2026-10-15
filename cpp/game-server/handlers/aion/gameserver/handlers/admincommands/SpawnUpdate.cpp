#include "aion/gameserver/handlers/admincommands/SpawnUpdate.h"

#include <typeinfo>
#include <vector>

#include "aion/commons/configuration/transformers/NumberTransformer.h"
#include "aion/commons/utils/Numbers.h"
#include "aion/commons/utils/StringUtils.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/SpawnsData.h"
#include "aion/gameserver/dataholders/WalkerData.h"
#include "aion/gameserver/geoEngine/math/JavaFloat.h"
#include "aion/gameserver/model/gameobjects/Gatherable.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/templates/spawns/SpawnGroup.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/model/templates/walker/WalkerTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_DELETE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_GATHERABLE_INFO.h"
#include "aion/gameserver/network/aion/serverpackets/SM_NPC_INFO.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/utils/SimpleClassName.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::handlers::admincommands {

AION_ADMIN_COMMAND(SpawnUpdate);

// Java SpawnUpdate.java:26-33
SpawnUpdate::SpawnUpdate()
	: AdminCommand("spawnu", "Updates spawn data.",
		  "<x|y|z|h> [value] - Update X, Y, or Z coordinate or the heading of the selected NPC/gatherable (default: takes your current position, optional: the specified value).\n"
		  "<xyz|xyzh> - Update position or position and heading of the selected NPC/gatherable to your own one.\n"
		  "w [walker_id] - Set walker data of the selected NPC (default: remove walker data, optional: set NPC's walker ID).\n") {
}

// Java SpawnUpdate.java:35-110
void SpawnUpdate::execute(Player& admin, std::span<const std::string> params) {
	using commons::utils::StringUtils::equalsIgnoreCase; // parity: C++ using-declaration
	using commons::configuration::transformers::NumberParser::parseFloat; // parity: C++ using-declaration
	using geoEngine::math::JavaFloat; // parity: C++ using-declaration
	if (params.empty()) {
		sendInfo(admin);
		return;
	}

	runtime::Ptr<VisibleObject> target = admin.getTarget();
	if (runtime::as<Npc>(target) != nullptr && equalsIgnoreCase(params[0], "w")) { // parity= if (target instanceof Npc && params[0].equalsIgnoreCase("w")) {
		updateWalker(admin, *runtime::as<Npc>(target), params.size() == 2 ? std::optional<std::string>(commons::utils::StringUtils::toUpperCase(params[1])) : std::nullopt); // parity= updateWalker(admin, (Npc) target, params.length == 2 ? params[1].toUpperCase() : null);
		return;
	}

	if (runtime::as<Npc>(target) == nullptr && runtime::as<model::gameobjects::Gatherable>(target) == nullptr) { // parity= if (!(target instanceof Npc) && !(target instanceof Gatherable)) {
		PacketSendUtility::sendPacket(admin, SM_SYSTEM_MESSAGE::STR_INVALID_TARGET());
		return;
	}

	std::optional<float> x, y, z; // parity= Float x = null, y = null, z = null;
	std::optional<int8_t> h; // parity= Byte h = null;
	if (equalsIgnoreCase(params[0], "xyz")) { // parity= if (params[0].equalsIgnoreCase("xyz")) {
		x = admin.getX();
		y = admin.getY();
		z = admin.getZ();
	} else if (equalsIgnoreCase(params[0], "xyzh")) { // parity= } else if (params[0].equalsIgnoreCase("xyzh")) {
		x = admin.getX();
		y = admin.getY();
		z = admin.getZ();
		h = admin.getHeading();
	} else if (equalsIgnoreCase(params[0], "x")) { // parity= } else if (params[0].equalsIgnoreCase("x")) {
		if (params.size() == 1)
			x = admin.getX();
		else
			x = parseFloat(params[1]); // parity= x = Float.parseFloat(params[1]);
	} else if (equalsIgnoreCase(params[0], "y")) { // parity= } else if (params[0].equalsIgnoreCase("y")) {
		if (params.size() == 1)
			y = admin.getY();
		else
			y = parseFloat(params[1]); // parity= y = Float.parseFloat(params[1]);
	} else if (equalsIgnoreCase(params[0], "z")) { // parity= } else if (params[0].equalsIgnoreCase("z")) {
		if (params.size() == 1)
			z = admin.getZ();
		else
			z = parseFloat(params[1]); // parity= z = Float.parseFloat(params[1]);
	} else if (equalsIgnoreCase(params[0], "h")) { // parity= } else if (params[0].equalsIgnoreCase("h")) {
		if (params.size() == 1)
			h = admin.getHeading();
		else {
			int32_t parsed = commons::utils::parseInt(params[1]); // parity= h = Byte.parseByte(params[1]);
			if (parsed < -128 || parsed > 127) // parity: Byte.parseByte's range check
				throw commons::utils::NumberFormatException("Value out of range. Value:\"" + params[1] + "\" Radix:10"); // parity: (the same)
			h = static_cast<int8_t>(parsed); // parity: (the same)
		}
	} else {
		sendInfo(admin);
		return;
	}

	runtime::Ptr<world::WorldPosition> tPos = target->getPosition(); // parity= WorldPosition tPos = target.getPosition();
	tPos->setXYZH(x, y, z, h);

	if (runtime::Ptr<Npc> npc = runtime::as<Npc>(target)) // parity= if (target instanceof Npc npc)
		PacketSendUtility::sendPacket(admin, SM_NPC_INFO(*npc, admin)); // parity= PacketSendUtility.sendPacket(admin, new SM_NPC_INFO(npc, admin));
	else
		PacketSendUtility::sendPacket(admin, SM_GATHERABLE_INFO(*target)); // parity= PacketSendUtility.sendPacket(admin, new SM_GATHERABLE_INFO(target));
	sendInfo(admin,
		"Updated " + utils::simpleClassName(typeid(*target)) + "'s coordinates to\nX:" + JavaFloat::toString(tPos->getX()) + " Y:" + JavaFloat::toString(tPos->getY()) + " Z:" + JavaFloat::toString(tPos->getZ()) + " H:" // parity= "Updated " + target.getClass().getSimpleName() + "'s coordinates to\nX:" + tPos.getX() + " Y:" + tPos.getY() + " Z:" + tPos.getZ() + " H:"
			+ std::to_string(tPos->getHeading()) + "."); // parity= + tPos.getHeading() + ".");

	if (!dataholders::DataManager::SPAWNS_DATA->saveSpawn(*target, false)) // parity= if (!DataManager.SPAWNS_DATA.saveSpawn(target, false))
		sendInfo(admin, "Could not save spawn. Maybe it's a special or temporary spawn (siege, base, invasion, ...) which cannot be altered.");
}

// Java SpawnUpdate.java:112-142
void SpawnUpdate::updateWalker(Player& admin, Npc& target, std::optional<std::string> walkerId) {
	runtime::Ptr<SpawnTemplate> spawn = target.getSpawn(); // parity= SpawnTemplate spawn = target.getSpawn();
	std::optional<std::string> oldId = spawn->getWalkerId(); // parity= String oldId = spawn.getWalkerId();
	if (!oldId && !walkerId) { // parity= if (oldId == null && walkerId == null) {
		sendInfo(admin, "Npc has no walker_id that could be removed.");
	} else {
		if (walkerId) { // parity= if (walkerId != null) {
			const model::templates::walker::WalkerTemplate* walkerTemplate = dataholders::DataManager::WALKER_DATA->getWalkerTemplate(*walkerId); // parity= WalkerTemplate template = DataManager.WALKER_DATA.getWalkerTemplate(walkerId);
			if (walkerTemplate == nullptr) { // parity= if (template == null) {
				sendInfo(admin, "No such template exists in npc_walker.xml.");
				return;
			}
			std::vector<runtime::Ref<SpawnGroup>> allSpawns = dataholders::DataManager::SPAWNS_DATA->getSpawnsByWorldId(target.getWorldId()); // parity= List<SpawnGroup> allSpawns = DataManager.SPAWNS_DATA.getSpawnsByWorldId(target.getWorldId());
			std::vector<runtime::Ptr<SpawnTemplate>> allSpots; // parity= List<SpawnTemplate> allSpots = allSpawns.stream().flatMap(s -> s.getSpawnTemplates().stream()).toList();
			for (const runtime::Ref<SpawnGroup>& s : allSpawns) // parity: (the stream above)
				for (runtime::Ptr<SpawnTemplate> spot : s->getSpawnTemplates().snapshot()) // parity: (the same)
					allSpots.push_back(spot); // parity: (the same)
			std::vector<runtime::Ptr<SpawnTemplate>> sameIds; // parity= List<SpawnTemplate> sameIds = allSpots.stream().filter(s -> s.getWalkerId().equals(walkerId)).toList();
			for (const runtime::Ptr<SpawnTemplate>& s : allSpots) { // parity: (the stream above)
				std::optional<std::string> spotWalkerId = s->getWalkerId(); // parity: (the same)
				// correction J-S4CP4-1 (owner's decision 2026-10-13): walkerId.equals(s.getWalkerId()); a spot without walker id is not counted
				// (Java: s.getWalkerId().equals(walkerId), a NullPointerException)
				if (spotWalkerId && *spotWalkerId == *walkerId) // parity: (the same)
					sameIds.push_back(s); // parity: (the same)
			}
			if (static_cast<int32_t>(sameIds.size()) >= walkerTemplate->getPool()) { // parity= if (sameIds.size() >= template.getPool()) {
				sendInfo(admin, "Can not assign, walker pool reached the limit.");
				return;
			}
		}
		spawn->setWalkerId(walkerId);
		PacketSendUtility::sendPacket(admin, SM_DELETE(target)); // parity= PacketSendUtility.sendPacket(admin, new SM_DELETE(target));
		PacketSendUtility::sendPacket(admin, SM_NPC_INFO(target, admin)); // parity= PacketSendUtility.sendPacket(admin, new SM_NPC_INFO(target, admin));
		if (!walkerId) // parity= if (walkerId == null)
			sendInfo(admin, "Removed walker_id " + *oldId + " for NPC " + std::to_string(target.getNpcId()) + "."); // parity= sendInfo(admin, "Removed walker_id " + oldId + " for NPC " + target.getNpcId() + ".");
		else
			sendInfo(admin, "Updated walker_id from " + oldId.value_or("null") + " to " + *walkerId + " for NPC " + std::to_string(target.getNpcId()) + "."); // parity= sendInfo(admin, "Updated walker_id from " + oldId + " to " + walkerId + " for NPC " + target.getNpcId() + ".");
		if (!dataholders::DataManager::SPAWNS_DATA->saveSpawn(target, false)) // parity= if (!DataManager.SPAWNS_DATA.saveSpawn(target, false))
			sendInfo(admin, "Could not save spawn.");
	}
}

} // namespace aion::gameserver::handlers::admincommands
