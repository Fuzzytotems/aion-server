#include "aion/gameserver/utils/PacketSendUtility.h"

#include "aion/gameserver/ai/AbstractAI.h"
#include "aion/gameserver/model/ChatType.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/VisibleObject.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/team/legion/Legion.h"
#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/serverpackets/SM_MESSAGE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/sched/Pin.h"
#include "aion/gameserver/utils/PositionUtil.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"
#include "aion/gameserver/world/World.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"
#include "aion/gameserver/world/knownlist/KnownList.h"
#include "aion/gameserver/world/zone/ZoneInstance.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::utils {

using model::gameobjects::Creature;
using model::gameobjects::Npc;
using model::gameobjects::VisibleObject;
using model::gameobjects::player::Player;
using network::aion::AionServerPacket;
using network::aion::serverpackets::SM_MESSAGE;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;

void PacketSendUtility::sendMessage(Player& player, std::string_view msg) {
	sendPacket(player, SM_MESSAGE(0, "", msg, model::ChatType::GOLDEN_YELLOW)); // Java: senderName null (written as an empty string)
}

void PacketSendUtility::sendMessage(Player& player, std::string_view msg, model::ChatType chatType) {
	sendPacket(player, SM_MESSAGE(0, "", msg, chatType));
}

void PacketSendUtility::sendMonologue(Player& player, int32_t msgId, std::vector<std::string> params) {
	sendPacket(player, SM_SYSTEM_MESSAGE(model::ChatType::NORMAL, runtime::Ptr<VisibleObject>(player), msgId, std::move(params)));
}

void PacketSendUtility::sendMessage(Player& player, Npc& npc, int32_t msgId, std::vector<std::string> params) {
	sendPacket(player, SM_SYSTEM_MESSAGE(model::ChatType::NPC, runtime::Ptr<VisibleObject>(npc), msgId, std::move(params)));
}

void PacketSendUtility::broadcastMessage(runtime::Ptr<Npc> npc, int32_t msgId) {
	broadcastMessage(npc, msgId, 0, std::vector<std::string>());
}

// task lambda com.aionemu.gameserver.utils.PacketSendUtility@L65:17
void PacketSendUtility::broadcastMessage(runtime::Ptr<Npc> npc, int32_t msgId, int32_t delay, std::vector<std::string> msgParams) {
	if (!npc)
		return;
	Npc& speaker = *npc;
	scheduleOrRun(runtime::PinnedCallback<void()>(runtime::Pin{&speaker}, [&speaker, msgId, msgParams = std::move(msgParams)] {
		if (speaker.isSpawned()) {
			broadcastPacket(speaker, SM_SYSTEM_MESSAGE(model::ChatType::NPC, runtime::Ptr<VisibleObject>(speaker), msgId, msgParams),
				[&speaker](Player& player) { return PositionUtil::isInRange(speaker, player, 50, false); });
		}
	}),
		delay);
}

void PacketSendUtility::sendPacket(Player& player, AionServerPacket& packet) {
	// Deviation: Java `if (player.isOnline()) player.getClientConnection().sendPacket(packet)` loads the connection twice (isOnline tests the
	// same field), so a concurrent logout between the loads throws NullPointerException; the connection is loaded once (runtime-architecture.md
	// §8.3)
	std::shared_ptr<network::aion::AionConnection> connection = player.getClientConnection();
	if (connection)
		connection->sendPacket(packet);
}

void PacketSendUtility::broadcastPacket(Player& player, AionServerPacket& packet, bool toSelf) {
	if (toSelf)
		sendPacket(player, packet);
	broadcastPacket(static_cast<VisibleObject&>(player), packet);
}

void PacketSendUtility::broadcastPacket(VisibleObject& object, AionServerPacket& packet) {
	object.getKnownList().forEachPlayer([&packet](Player& player) { sendPacket(player, packet); });
}

void PacketSendUtility::broadcastPacket(VisibleObject& object, AionServerPacket& packet, const std::function<bool(Player&)>& filter) {
	object.getKnownList().forEachPlayer([&packet, &filter](Player& player) {
		if (filter(player))
			sendPacket(player, packet);
	});
}

void PacketSendUtility::broadcastPacketAndReceive(VisibleObject& visibleObject, AionServerPacket& packet) {
	if (auto* player = dynamic_cast<Player*>(&visibleObject))
		sendPacket(*player, packet);
	broadcastPacket(visibleObject, packet);
}

void PacketSendUtility::broadcastPacketAndReceive(Creature& creature, AionServerPacket& packet, std::optional<ai::event::AIEventType> et) {
	if (auto* player = dynamic_cast<Player*>(&creature))
		sendPacket(*player, packet);
	broadcastPacketAndAIEvent(creature, packet, et);
}

void PacketSendUtility::broadcastPacketAndAIEvent(Creature& creature, AionServerPacket& packet, std::optional<ai::event::AIEventType> et) {
	creature.getKnownList().forEachObject([&creature, &packet, et](VisibleObject& object) {
		if (auto* player = dynamic_cast<Player*>(&object))
			sendPacket(*player, packet);
		else if (et) {
			if (auto* npc = dynamic_cast<Npc*>(&object))
				npc->getAi().onCreatureEvent(*et, creature);
		}
	});
}

void PacketSendUtility::broadcastPacket(VisibleObject& object, AionServerPacket& packet, bool toSelf, const std::function<bool(Player&)>& filter) {
	if (toSelf) {
		if (auto* player = dynamic_cast<Player*>(&object))
			sendPacket(*player, packet);
	}
	object.getKnownList().forEachPlayer([&packet, &filter](Player& player) {
		if (filter(player))
			sendPacket(player, packet);
	});
}

void PacketSendUtility::broadcastToWorld(AionServerPacket& packet) {
	world::World::getInstance().forEachPlayer([&packet](Player& player) { sendPacket(player, packet); });
}

void PacketSendUtility::broadcastToWorld(AionServerPacket& packet, const std::function<bool(Player&)>& filter) {
	world::World::getInstance().forEachPlayer([&packet, &filter](Player& player) {
		if (filter(player))
			sendPacket(player, packet);
	});
}

void PacketSendUtility::broadcastToLegion(model::team::legion::Legion& legion, AionServerPacket& packet) {
	for (const runtime::Ptr<Player>& onlineLegionMember : legion.getOnlinePlayers())
		sendPacket(*onlineLegionMember, packet);
}

void PacketSendUtility::broadcastToLegion(model::team::legion::Legion& legion, AionServerPacket& packet, int32_t playerObjId) {
	for (const runtime::Ptr<Player>& onlineLegionMember : legion.getOnlinePlayers()) {
		if (onlineLegionMember->getObjectId() != playerObjId)
			sendPacket(*onlineLegionMember, packet);
	}
}

void PacketSendUtility::broadcastToSightedPlayers(VisibleObject& object, AionServerPacket& packet) {
	broadcastToSightedPlayers(object, packet, false);
}

void PacketSendUtility::broadcastToSightedPlayers(VisibleObject& object, AionServerPacket& packet, bool toSelf) {
	broadcastPacket(object, packet, toSelf, [&object](Player& other) { return other.getKnownList().sees(object); });
}

// filter lambda com.aionemu.gameserver.utils.PacketSendUtility@L220:95
void PacketSendUtility::broadcastToMap(VisibleObject& object, int32_t msgId) {
	SM_SYSTEM_MESSAGE packet(msgId, std::vector<std::string>());
	sendToMapPlayers(*object.getPosition()->getWorldMapInstance(), packet, [&object](Player& p) { return !object.equals(p); });
}

// filter lambda com.aionemu.gameserver.utils.PacketSendUtility@L227:99
void PacketSendUtility::broadcastToMap(VisibleObject& object, int32_t msgId, int32_t delay) {
	broadcastToMap(*object.getPosition()->getWorldMapInstance(), std::make_shared<SM_SYSTEM_MESSAGE>(msgId, std::vector<std::string>()), delay,
		runtime::PinnedCallback<bool(Player&)>(runtime::Pin{&object}, [&object](Player& p) { return !object.equals(p); }));
}

// filter lambda com.aionemu.gameserver.utils.PacketSendUtility@L234:73
void PacketSendUtility::broadcastToMap(VisibleObject& object, AionServerPacket& packet) {
	sendToMapPlayers(*object.getPosition()->getWorldMapInstance(), packet, [&object](Player& p) { return !object.equals(p); });
}

// filter lambda com.aionemu.gameserver.utils.PacketSendUtility@L241:77
void PacketSendUtility::broadcastToMap(VisibleObject& object, std::shared_ptr<AionServerPacket> packet, int32_t delay) {
	broadcastToMap(*object.getPosition()->getWorldMapInstance(), std::move(packet), delay,
		runtime::PinnedCallback<bool(Player&)>(runtime::Pin{&object}, [&object](Player& p) { return !object.equals(p); }));
}

void PacketSendUtility::broadcastToMap(VisibleObject& object, std::shared_ptr<AionServerPacket> packet, int32_t delay,
	runtime::PinnedCallback<bool(Player&)> filter) {
	broadcastToMap(*object.getPosition()->getWorldMapInstance(), std::move(packet), delay, std::move(filter));
}

// filter lambda com.aionemu.gameserver.utils.PacketSendUtility@L256:42
void PacketSendUtility::broadcastToMap(world::WorldMapInstance& mapInstance, AionServerPacket& packet) {
	sendToMapPlayers(mapInstance, packet, [](Player&) { return true; });
}

// filter lambda com.aionemu.gameserver.utils.PacketSendUtility@L263:46
void PacketSendUtility::broadcastToMap(world::WorldMapInstance& mapInstance, std::shared_ptr<AionServerPacket> packet, int32_t delay) {
	broadcastToMap(mapInstance, std::move(packet), delay, runtime::PinnedCallback<bool(Player&)>([](Player&) { return true; }));
}

// task lambda com.aionemu.gameserver.utils.PacketSendUtility@L270:17
void PacketSendUtility::broadcastToMap(world::WorldMapInstance& mapInstance, std::shared_ptr<AionServerPacket> packet, int32_t delay,
	runtime::PinnedCallback<bool(Player&)> filter) {
	scheduleOrRun(runtime::PinnedCallback<void()>(runtime::Pin{&mapInstance}, [&mapInstance, packet = std::move(packet), filter = std::move(filter)] {
		sendToMapPlayers(mapInstance, *packet, [&filter](Player& player) { return filter(player); });
	}),
		delay);
}

// task lambda com.aionemu.gameserver.utils.PacketSendUtility@L277:17
void PacketSendUtility::broadcastToZone(world::zone::ZoneInstance& zone, AionServerPacket& packet) {
	// Java: scheduleOrRun(task, 0), which runs the task inline
	zone.forEach([&packet](Creature& creature) {
		if (auto* player = dynamic_cast<Player*>(&creature))
			sendPacket(*player, packet);
	});
}

void PacketSendUtility::scheduleOrRun(runtime::PinnedCallback<void()> r, int32_t delay) {
	if (delay <= 0)
		r();
	else
		ThreadPoolManager::getInstance().schedule(runtime::Pin(), [r = std::move(r)] { r(); }, delay);
}

void PacketSendUtility::sendToMapPlayers(world::WorldMapInstance& mapInstance, AionServerPacket& packet,
	const std::function<bool(Player&)>& filter) {
	mapInstance.forEachPlayer([&packet, &filter](Player& player) {
		if (filter(player))
			sendPacket(player, packet);
	});
}

} // namespace aion::gameserver::utils
