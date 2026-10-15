#include "aion/gameserver/services/rift/RiftInformer.h"

#include "aion/gameserver/controllers/RVController.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/serverpackets/SM_RIFT_ANNOUNCE.h"
#include "aion/gameserver/services/rift/RiftEnum.h"
#include "aion/gameserver/services/rift/RiftManager.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/world/World.h"
#include "aion/gameserver/world/WorldMap.h"
#include "aion/gameserver/world/WorldMapInstance.h"

namespace aion::gameserver::services::rift {

using network::aion::AionServerPacket;
using network::aion::serverpackets::SM_RIFT_ANNOUNCE;
using PacketList = std::vector<std::unique_ptr<AionServerPacket>>;

/** Java (RVController) rift.getController(): ClassCastException (std::bad_cast) for another controller */
static controllers::RVController& riftController(model::gameobjects::Npc& rift) {
	return dynamic_cast<controllers::RVController&>(rift.getController());
}

void RiftInformer::sendRiftsInfo(int32_t worldId) {
	syncRiftsState(worldId, getPackets(worldId));
	int32_t twinId = getTwinId(worldId);
	if (twinId > 0)
		syncRiftsState(twinId, getPackets(twinId));
}

void RiftInformer::sendRiftsInfo(model::gameobjects::player::Player& player) {
	syncRiftsState(player, getPackets(player.getWorldId()));
	int32_t twinId = getTwinId(player.getWorldId());
	if (twinId > 0)
		syncRiftsState(twinId, getPackets(twinId));
}

void RiftInformer::sendRiftInfo(std::span<const int32_t> worlds) {
	for (int32_t worldId : worlds)
		syncRiftsState(worldId, getPackets(worlds[0], -1)); // Java: worlds[0] (ArrayIndexOutOfBoundsException for an empty array cannot happen here)
}

void RiftInformer::sendRiftDespawn(int32_t worldId, int32_t objId) {
	syncRiftsState(worldId, getPackets(worldId, objId), true);
}

PacketList RiftInformer::getPackets(int32_t worldId) {
	return getPackets(worldId, 0);
}

PacketList RiftInformer::getPackets(int32_t worldId, int32_t objId) {
	PacketList packets;
	if (objId == -1) {
		for (const runtime::Ptr<model::gameobjects::Npc>& rift : RiftManager::getSpawnedRifts(worldId)) {
			controllers::RVController& controller = riftController(*rift);
			if (!controller.isMaster()) {
				continue;
			}

			packets.push_back(std::make_unique<SM_RIFT_ANNOUNCE>(controller, false));
		}
	} else if (objId > 0) {
		packets.push_back(std::make_unique<SM_RIFT_ANNOUNCE>(objId));
	} else {
		packets.push_back(std::make_unique<SM_RIFT_ANNOUNCE>(getAnnounceData(worldId)));
		for (const runtime::Ptr<model::gameobjects::Npc>& rift : RiftManager::getSpawnedRifts(worldId)) {
			controllers::RVController& controller = riftController(*rift);
			if (!controller.isMaster()) {
				continue;
			}
			packets.push_back(std::make_unique<SM_RIFT_ANNOUNCE>(controller, true));
			packets.push_back(std::make_unique<SM_RIFT_ANNOUNCE>(controller, false));
		}
	}
	return packets;
}

/*
 * Sends generated rift info packets to player
 */
void RiftInformer::syncRiftsState(model::gameobjects::player::Player& player, const PacketList& packets) {
	for (const std::unique_ptr<AionServerPacket>& packet : packets) {
		utils::PacketSendUtility::sendPacket(player, *packet);
	}
}

/*
 * Sends generated rift info packets to all players within world
 */
void RiftInformer::syncRiftsState(int32_t worldId, const PacketList& packets) {
	syncRiftsState(worldId, packets, false);
}

void RiftInformer::syncRiftsState(int32_t worldId, const PacketList& packets, bool isDespawnInfo) {
	// Java: World.getInstance().getWorldMap(worldId).getMainWorldMapInstance().forEachPlayer(...) (NullPointerException for an unknown map)
	world::World::getInstance().getWorldMap(worldId)->getMainWorldMapInstance()->forEachPlayer(
		[&packets](model::gameobjects::player::Player& player) { syncRiftsState(player, packets); });
}

std::map<int32_t, int32_t> RiftInformer::getAnnounceData(int32_t worldId) {
	std::map<int32_t, int32_t> localRifts;

	/**
	 * init empty list
	 * 12 different announces
	 * we have to send them all otherwise the client announces them even if they are not spawned
	 * looks like it depends on which map you are. But its always: first 6 indexes are from [current map] to [other map].
	 * whereas the last 6 indexes are from [other map] to [current map].
	 * index - announce
	 * 0 - normal rift
	 * 1 - vortex rift opened
	 * 2 - rift to concert hall
	 * 3 - rift to pangaea/ahserion
	 * 4 - volatile rift
	 * 5 - infiltration rift
	 *
	 * 6 - normal rift
	 * 7 - vortex rift opened
	 * 8 - rift to concert hall
	 * 9 - rift to pangaea/ahserion
	 * 10 - volatile rift
	 * 11 - infiltration rift
	 */
	for (int32_t i = 0; i < 12; i++) {
		localRifts[i] = 0;
	}

	for (const runtime::Ptr<model::gameobjects::Npc>& rift : RiftManager::getSpawnedRifts(worldId)) {
		controllers::RVController& rc = riftController(*rift);
		localRifts = calcRiftsData(rc, localRifts);
	}

	return localRifts;
}

std::map<int32_t, int32_t> RiftInformer::calcRiftsData(controllers::RVController& rift, std::map<int32_t, int32_t>& local) {
	if (rift.isMaster()) {
		switch (rift.getRiftTemplate()) {
			case RiftEnum::MARCHUTAN_AM:
			case RiftEnum::KAISINEL_AM:
				local[1] = local.at(1) + 1; // vortex rift
				break;
			case RiftEnum::MORHEIM_AM:
			case RiftEnum::MORHEIM_BM:
			case RiftEnum::MORHEIM_CM:
			case RiftEnum::MORHEIM_DM:
			case RiftEnum::MORHEIM_EM:
			case RiftEnum::MORHEIM_FM:
			case RiftEnum::MORHEIM_GM:
			case RiftEnum::BELUSLAN_AM:
			case RiftEnum::BELUSLAN_BM:
			case RiftEnum::BELUSLAN_CM:
			case RiftEnum::BELUSLAN_DM:
			case RiftEnum::BELUSLAN_EM:
			case RiftEnum::BELUSLAN_FM:
			case RiftEnum::BELUSLAN_GM:
			case RiftEnum::GELKMAROS_AM:
			case RiftEnum::GELKMAROS_BM:
			case RiftEnum::GELKMAROS_CM:
			case RiftEnum::GELKMAROS_DM:
			case RiftEnum::ENSHAR_AM:
			case RiftEnum::ENSHAR_BM:
			case RiftEnum::ENSHAR_CM:
			case RiftEnum::ENSHAR_DM:
			case RiftEnum::ENSHAR_EM:
			case RiftEnum::ENSHAR_FM:
			case RiftEnum::ELTNEN_AM:
			case RiftEnum::ELTNEN_BM:
			case RiftEnum::ELTNEN_CM:
			case RiftEnum::ELTNEN_DM:
			case RiftEnum::ELTNEN_EM:
			case RiftEnum::ELTNEN_FM:
			case RiftEnum::ELTNEN_GM:
			case RiftEnum::HEIRON_AM:
			case RiftEnum::HEIRON_BM:
			case RiftEnum::HEIRON_CM:
			case RiftEnum::HEIRON_DM:
			case RiftEnum::HEIRON_EM:
			case RiftEnum::HEIRON_FM:
			case RiftEnum::HEIRON_GM:
			case RiftEnum::INGGISON_AM:
			case RiftEnum::INGGISON_BM:
			case RiftEnum::INGGISON_CM:
			case RiftEnum::INGGISON_DM:
			case RiftEnum::CYGNEA_AM:
			case RiftEnum::CYGNEA_BM:
			case RiftEnum::CYGNEA_CM:
			case RiftEnum::CYGNEA_DM:
			case RiftEnum::CYGNEA_EM:
			case RiftEnum::CYGNEA_FM:
				local[0] = local.at(0) + 1; // normal rift
				break;
			case RiftEnum::ENSHAR_GM:
			case RiftEnum::ENSHAR_HM:
			case RiftEnum::ENSHAR_IM:
			case RiftEnum::CYGNEA_GM:
			case RiftEnum::CYGNEA_HM:
			case RiftEnum::CYGNEA_IM:
				if (rift.isVolatile()) {
					local[4] = local.at(4) + 1; // chaos rift
				} else {
					local[0] = local.at(0) + 1; // normal rift
				}
				break;
			default:
				break;
		}
	}
	return local;
}

int32_t RiftInformer::getTwinId(int32_t worldId) {
	switch (worldId) {
		case 110070000: // Kaisinel Academy -> Brusthonin
			return 220050000;
		case 210020000: // Eltnen -> Morheim
			return 220020000;
		case 210040000: // Heiron -> Beluslan
			return 220040000;
		case 210050000: // Inggison -> Gelkmaros
			return 220070000;
		case 210060000: // Theobomos -> Marchutan Priory
			return 120080000;
		case 210070000: // Cygnea -> Enshar
			return 220080000;
		case 120080000: // Marchutan Priory -> Theobomos
			return 210060000;
		case 220020000: // Morheim -> Eltnen
			return 210020000;
		case 220040000: // Beluslan -> Heiron
			return 210040000;
		case 220050000: // Brusthonin -> Kaisinel Academy
			return 110070000;
		case 220070000: // Gelkmaros -> Inggison
			return 210050000;
		case 220080000: // Enshar -> Cygnea
			return 210070000;
		default:
			return 0;
	}
}

} // namespace aion::gameserver::services::rift
