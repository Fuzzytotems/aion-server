#include "aion/gameserver/network/aion/clientpackets/CM_HOUSE_TELEPORT.h"

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/ai/AbstractAI.h"
#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/model/animations/TeleportAnimation.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/player/Friend.h"
#include "aion/gameserver/model/gameobjects/player/FriendList.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/house/House.h"
#include "aion/gameserver/model/team/legion/Legion.h"
#include "aion/gameserver/model/templates/npc/NpcTemplateType.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/services/HousingService.h"
#include "aion/gameserver/services/instance/InstanceService.h"
#include "aion/gameserver/services/teleport/TeleportService.h"
#include "aion/gameserver/utils/audit/AuditLogger.h"
#include "aion/gameserver/world/WorldMapInstance.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

CM_HOUSE_TELEPORT::CM_HOUSE_TELEPORT(int32_t opcode, const StateSet& validStates) : AionClientPacket(opcode, validStates) {
}

// Java CM_HOUSE_TELEPORT.java:36-40
void CM_HOUSE_TELEPORT::readImpl() {
	actionId = readUC();
	playerId1 = readD(); // just why? without this field we wouldn't even have to check exploitations
	playerId2 = readD();
}

// Java CM_HOUSE_TELEPORT.java:43-91
void CM_HOUSE_TELEPORT::runImpl() {
	const runtime::Ptr<model::gameobjects::player::Player> player = getConnection()->getActivePlayer();
	if (!player)
		return;
	if (playerId1 != player->getObjectId()) {
		utils::audit::AuditLogger::log(*player, "tried to teleport playerId " + std::to_string(playerId1) + " instead of himself");
		return;
	}

	const runtime::Ptr<model::gameobjects::VisibleObject> target = player->getTarget();
	const runtime::Ptr<model::gameobjects::Npc> relationshipCrystal = runtime::as<model::gameobjects::Npc>(target);
	if (!relationshipCrystal)
		return;
	if (relationshipCrystal->getNpcTemplateType() != model::templates::npc::NpcTemplateType::HOUSING ||
		relationshipCrystal->getAi().getName() != "friendportal") {
		utils::audit::AuditLogger::log(*player, "tried to use house teleport without targeting a relationship crystal: " + target->toString());
		return;
	}

	runtime::Ptr<model::house::House> house;
	switch (actionId) {
		case 1: // to own house
			house = player->getActiveHouse();
			break;
		case 2: { // to friends house
			if (playerId2 == 0)
				return;
			for (const runtime::Ptr<model::house::House>& h : findFriendsAccessibleHouses(*player)) { // Java: stream().filter().findAny()
				if (h->getOwnerId() == playerId2) {
					house = h;
					break;
				}
			}
			break;
		}
		case 3: { // to random friend's house
			std::vector<runtime::Ptr<model::house::House>> houses = findFriendsAccessibleHouses(*player);
			const runtime::Ptr<model::house::House>* chosen = commons::utils::Rnd::get(houses);
			house = chosen == nullptr ? nullptr : *chosen;
			if (!house) {
				sendPacket(serverpackets::SM_SYSTEM_MESSAGE::STR_MSG_NO_RELATIONSHIP_RECENTLY());
				return;
			}
			break;
		}
		default:
			commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.network.aion.clientpackets.CM_HOUSE_TELEPORT")
				.warn("Unhandled house teleport actionId " + std::to_string(actionId));
			return;
	}

	if (!house)
		return;

	const runtime::Ptr<world::WorldMapInstance> instance = services::instance::InstanceService::getOrCreateHouseInstance(*house);
	services::teleport::TeleportService::teleportTo(*player, *instance, house->getX(), house->getY(), house->getZ(), house->getTeleportHeading(),
		model::animations::TeleportAnimation::FADE_OUT_BEAM);
}

// Java CM_HOUSE_TELEPORT.java:93-105
std::vector<runtime::Ptr<model::house::House>> CM_HOUSE_TELEPORT::findFriendsAccessibleHouses(model::gameobjects::player::Player& player) {
	std::vector<runtime::Ptr<model::house::House>> houses;
	for (const runtime::Ptr<model::gameobjects::player::Friend>& friend_ : player.getFriendList())
		addHouseIfAccessible(player, houses, friend_->getObjectId());
	const runtime::Ptr<model::team::legion::Legion> legion = player.getLegion();
	if (legion) {
		for (int32_t memberId : *legion->getMemberIds()) {
			if (memberId != player.getObjectId())
				addHouseIfAccessible(player, houses, memberId);
		}
	}
	return houses;
}

// Java CM_HOUSE_TELEPORT.java:107-111
void CM_HOUSE_TELEPORT::addHouseIfAccessible(model::gameobjects::player::Player& player, std::vector<runtime::Ptr<model::house::House>>& relationIds,
	int32_t friendId) {
	const runtime::Ptr<model::house::House> house = services::HousingService::getInstance().findActiveHouse(friendId);
	if (house && house->canEnter(player))
		relationIds.push_back(house);
}

AION_CLIENT_PACKET(CM_HOUSE_TELEPORT);

} // namespace aion::gameserver::network::aion::clientpackets
