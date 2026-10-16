#include "aion/gameserver/handlers/admincommands/Collide.h"

#include <optional>

#include "aion/commons/utils/StringUtils.h"
#include "aion/gameserver/geoEngine/collision/CollisionResult.h"
#include "aion/gameserver/geoEngine/collision/CollisionIntentionInfo.h"
#include "aion/gameserver/geoEngine/collision/CollisionResults.h"
#include "aion/gameserver/geoEngine/math/JavaFloat.h"
#include "aion/gameserver/geoEngine/math/Vector3f.h"
#include "aion/gameserver/geoEngine/scene/Geometry.h"
#include "aion/gameserver/geoEngine/scene/Node.h"
#include "aion/gameserver/model/templates/BoundRadius.h"
#include "aion/gameserver/model/templates/VisibleObjectTemplate.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/spawnengine/SpawnEngine.h"
#include "aion/gameserver/world/geo/GeoService.h"

namespace aion::gameserver::handlers::admincommands {

AION_ADMIN_COMMAND(Collide);

// Java Collide.java:23-28
Collide::Collide()
	: AdminCommand("collide", "Geo debugging tool.",
		  " - Lists collisions between your target and the ground.\n"
		  "me - Lists collisions between you and your target.\n") {
}

// Java Collide.java:30-104
void Collide::execute(Player& admin, std::span<const std::string> params) {
	using geoEngine::collision::CollisionResult; // parity: C++ using-declaration
	using geoEngine::collision::CollisionResults; // parity: C++ using-declaration
	using geoEngine::math::JavaFloat; // parity: C++ using-declaration
	bool isMe = false;
	if (params.size() > 0 && !(isMe = commons::utils::StringUtils::equalsIgnoreCase("me", params[0]))) { // parity= if (params.length > 0 && !(isMe = "me".equalsIgnoreCase(params[0]))) {
		sendInfo(admin);
		return;
	}
	runtime::Ptr<VisibleObject> target = admin.getTarget();
	if (target == nullptr) {
		PacketSendUtility::sendPacket(admin, SM_SYSTEM_MESSAGE::STR_INVALID_TARGET());
		return;
	}

	const int8_t intentions = geoEngine::collision::getId(CollisionIntention::PHYSICAL); // parity= final byte intentions = CollisionIntention.PHYSICAL.getId();
	float x = target->getX();
	float y = target->getY();
	float z = target->getZ();
	float targetX, targetY, targetZ;

	if (!isMe) {
		targetX = x;
		targetY = y;
		targetZ = z - 10;
	} else {
		targetX = admin.getX();
		targetY = admin.getY();
		targetZ = admin.getZ() + admin.getObjectTemplate()->getBoundRadius()->getUpper() / 6; // parity= targetZ = admin.getZ() + admin.getObjectTemplate().getBoundRadius().getUpper() / 6;
		sendInfo(admin, "From target towards you:");
	}

	sendInfo(admin, "Target: X=" + JavaFloat::toString(x) + "; Y=" + JavaFloat::toString(y) + "; Z=" + JavaFloat::toString(z)); // parity= sendInfo(admin, "Target: X=" + x + "; Y=" + y + "; Z=" + z);

	CollisionResults results = GeoService::getInstance().getCollisions(*target, targetX, targetY, targetZ, intentions, nullptr);
	std::optional<CollisionResult> closest = results.getClosestCollision(); // parity= CollisionResult closest = results.getClosestCollision();

	if (results.size() == 0) {
		sendInfo(admin, "No collisions found.");
		closest = std::nullopt; // parity= closest = null;
	} else {
		listCollisions(admin, results, closest.value()); // parity= listCollisions(admin, results, closest);
	}

	std::optional<CollisionResult> closestOpposite; // parity= CollisionResult closestOpposite = null;

	if (isMe) {
		sendInfo(admin, "From you towards your target:");
		sendInfo(admin, "Admin: X=" + JavaFloat::toString(admin.getX()) + "; Y=" + JavaFloat::toString(admin.getY()) + "; Z=" + JavaFloat::toString(admin.getZ())); // parity= sendInfo(admin, "Admin: X=" + admin.getX() + "; Y=" + admin.getY() + "; Z=" + admin.getZ());

		CollisionResults oppositeResults = GeoService::getInstance().getCollisions(admin, // parity= results = GeoService.getInstance().getCollisions(admin, target.getX(), target.getY(),
			target->getX(), target->getY(), // parity: (the same; CollisionResults has const members and is not assignable)
			target->getZ() + target->getObjectTemplate()->getBoundRadius()->getUpper() / 2, intentions, nullptr);
		closestOpposite = oppositeResults.getClosestCollision(); // parity= closestOpposite = results.getClosestCollision();

		if (oppositeResults.size() == 0) { // parity= if (results.size() == 0) {
			sendInfo(admin, "No collisions found.");
			closestOpposite = std::nullopt; // parity= closestOpposite = null;
		} else {
			listCollisions(admin, oppositeResults, closestOpposite.value()); // parity= listCollisions(admin, results, closestOpposite);
		}
	}

	if (!isMe && closest && closest->getContactPoint().z + 0.5f < target->getZ()) { // parity= if (!isMe && closest != null && closest.getContactPoint().z + 0.5f < target.getZ()) {
		sendInfo(admin, "Closest collision is below target's Z coordinate!");
	} else {
		if (closest) { // parity= if (closest != null) {
			runtime::Ref<SpawnTemplate> spawn = SpawnEngine::newSpawn(admin.getWorldId(), 200000, closest->getContactPoint().x, closest->getContactPoint().y, // parity= SpawnTemplate spawn = SpawnEngine.newSpawn(admin.getWorldId(), 200000, closest.getContactPoint().x, closest.getContactPoint().y,
				closest->getContactPoint().z, static_cast<int8_t>(0), 0); // parity= closest.getContactPoint().z, (byte) 0, 0);
			SpawnEngine::spawnObject(*spawn, admin.getInstanceId());
		}
		if (closestOpposite) { // parity= if (closestOpposite != null) {
			runtime::Ref<SpawnTemplate> spawn = SpawnEngine::newSpawn(admin.getWorldId(), 200000, closestOpposite->getContactPoint().x, // parity= SpawnTemplate spawn = SpawnEngine.newSpawn(admin.getWorldId(), 200000, closestOpposite.getContactPoint().x,
				closestOpposite->getContactPoint().y, closestOpposite->getContactPoint().z, static_cast<int8_t>(0), 0); // parity= closestOpposite.getContactPoint().y, closestOpposite.getContactPoint().z, (byte) 0, 0);
			SpawnEngine::spawnObject(*spawn, admin.getInstanceId());
		}
	}
}

// Java Collide.java:106-125 (the callers pass a non-null closest collision: results.size() > 0; the local description is renamed
// descriptionText, it would hide ChatCommand::description, C4458)
void Collide::listCollisions(Player& admin, geoEngine::collision::CollisionResults& results, const geoEngine::collision::CollisionResult& closestOpposite) {
	int32_t count = 1;
	int32_t closestId = 0;
	std::string descriptionText = ""; // parity= String description = "";

	for (auto iter = results.iterator(); iter.hasNext(); count++) { // parity= for (Iterator<CollisionResult> iter = results.iterator(); iter.hasNext(); count++) {
		geoEngine::collision::CollisionResult result = iter.next(); // parity= CollisionResult result = iter.next();
		if (result.equals(closestOpposite))
			closestId = count;
		if (result.getGeometry() == nullptr)
			descriptionText += std::to_string(count) + ". " + result.getContactPoint().toString() + "\n"; // parity= description += count + ". " + result.getContactPoint().toString() + "\n";
		else {
			if (result.getGeometry()->getName().empty()) { // parity= if (result.getGeometry().getName() == null) {
				descriptionText += std::to_string(count) + ". " + result.getContactPoint().toString() + "; parent=" + result.getGeometry()->getParent()->getName() + "\n"; // parity= description += count + ". " + result.getContactPoint().toString() + "; parent=" + result.getGeometry().getParent().getName() + "\n";
			} else
				descriptionText += std::to_string(count) + ". " + result.getContactPoint().toString() + "; name=" + result.getGeometry()->getName() + "\n"; // parity= description += count + ". " + result.getContactPoint().toString() + "; name=" + result.getGeometry().getName() + "\n";
		}
	}
	descriptionText += "-----------------------\nClosest: " + std::to_string(closestId) + ". Distance: " + geoEngine::math::JavaFloat::toString(closestOpposite.getDistance()); // parity= description += "-----------------------\nClosest: " + closestId + ". Distance: " + closestOpposite.getDistance();
	sendInfo(admin, descriptionText); // parity= sendInfo(admin, description);
}

} // namespace aion::gameserver::handlers::admincommands
