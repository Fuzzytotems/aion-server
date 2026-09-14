#include "aion/gameserver/utils/PacketSendUtility.h"

#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::utils {

void PacketSendUtility::sendMessage(model::gameobjects::player::Player& player, std::string_view msg) {
	AION_UNPORTED();
}

void PacketSendUtility::sendMessage(model::gameobjects::player::Player& player, std::string_view msg, model::ChatType chatType) {
	AION_UNPORTED();
}

void PacketSendUtility::sendMonologue(model::gameobjects::player::Player& player, int32_t msgId, std::vector<std::string> params) {
	AION_UNPORTED();
}

void PacketSendUtility::sendMessage(model::gameobjects::player::Player& player, model::gameobjects::Npc& npc, int32_t msgId,
	std::vector<std::string> params) {
	AION_UNPORTED();
}

void PacketSendUtility::broadcastMessage(runtime::Ptr<model::gameobjects::Npc> npc, int32_t msgId) {
	AION_UNPORTED();
}

// task lambda com.aionemu.gameserver.utils.PacketSendUtility@L65:17
void PacketSendUtility::broadcastMessage(runtime::Ptr<model::gameobjects::Npc> npc, int32_t msgId, int32_t delay,
	std::vector<std::string> msgParams) {
	AION_UNPORTED();
}

void PacketSendUtility::sendPacket(model::gameobjects::player::Player& player, network::aion::AionServerPacket& packet) {
	AION_UNPORTED();
}

void PacketSendUtility::broadcastPacket(model::gameobjects::player::Player& player, network::aion::AionServerPacket& packet, bool toSelf) {
	AION_UNPORTED();
}

void PacketSendUtility::broadcastPacket(model::gameobjects::VisibleObject& object, network::aion::AionServerPacket& packet) {
	AION_UNPORTED();
}

void PacketSendUtility::broadcastPacket(model::gameobjects::VisibleObject& object, network::aion::AionServerPacket& packet,
	const std::function<bool(model::gameobjects::player::Player&)>& filter) {
	AION_UNPORTED();
}

void PacketSendUtility::broadcastPacketAndReceive(model::gameobjects::VisibleObject& visibleObject, network::aion::AionServerPacket& packet) {
	AION_UNPORTED();
}

void PacketSendUtility::broadcastPacketAndReceive(model::gameobjects::Creature& creature, network::aion::AionServerPacket& packet,
	std::optional<ai::event::AIEventType> et) {
	AION_UNPORTED();
}

void PacketSendUtility::broadcastPacketAndAIEvent(model::gameobjects::Creature& creature, network::aion::AionServerPacket& packet,
	std::optional<ai::event::AIEventType> et) {
	AION_UNPORTED();
}

void PacketSendUtility::broadcastPacket(model::gameobjects::VisibleObject& object, network::aion::AionServerPacket& packet, bool toSelf,
	const std::function<bool(model::gameobjects::player::Player&)>& filter) {
	AION_UNPORTED();
}

void PacketSendUtility::broadcastToWorld(network::aion::AionServerPacket& packet) {
	AION_UNPORTED();
}

void PacketSendUtility::broadcastToWorld(network::aion::AionServerPacket& packet,
	const std::function<bool(model::gameobjects::player::Player&)>& filter) {
	AION_UNPORTED();
}

void PacketSendUtility::broadcastToLegion(model::team::legion::Legion& legion, network::aion::AionServerPacket& packet) {
	AION_UNPORTED();
}

void PacketSendUtility::broadcastToLegion(model::team::legion::Legion& legion, network::aion::AionServerPacket& packet, int32_t playerObjId) {
	AION_UNPORTED();
}

void PacketSendUtility::broadcastToSightedPlayers(model::gameobjects::VisibleObject& object, network::aion::AionServerPacket& packet) {
	AION_UNPORTED();
}

void PacketSendUtility::broadcastToSightedPlayers(model::gameobjects::VisibleObject& object, network::aion::AionServerPacket& packet, bool toSelf) {
	AION_UNPORTED();
}

// filter lambda com.aionemu.gameserver.utils.PacketSendUtility@L220:95
void PacketSendUtility::broadcastToMap(model::gameobjects::VisibleObject& object, int32_t msgId) {
	AION_UNPORTED();
}

// filter lambda com.aionemu.gameserver.utils.PacketSendUtility@L227:99
void PacketSendUtility::broadcastToMap(model::gameobjects::VisibleObject& object, int32_t msgId, int32_t delay) {
	AION_UNPORTED();
}

// filter lambda com.aionemu.gameserver.utils.PacketSendUtility@L234:73
void PacketSendUtility::broadcastToMap(model::gameobjects::VisibleObject& object, network::aion::AionServerPacket& packet) {
	AION_UNPORTED();
}

// filter lambda com.aionemu.gameserver.utils.PacketSendUtility@L241:77
void PacketSendUtility::broadcastToMap(model::gameobjects::VisibleObject& object, std::shared_ptr<network::aion::AionServerPacket> packet,
	int32_t delay) {
	AION_UNPORTED();
}

void PacketSendUtility::broadcastToMap(model::gameobjects::VisibleObject& object, std::shared_ptr<network::aion::AionServerPacket> packet,
	int32_t delay, runtime::PinnedCallback<bool(model::gameobjects::player::Player&)> filter) {
	AION_UNPORTED();
}

// filter lambda com.aionemu.gameserver.utils.PacketSendUtility@L256:42
void PacketSendUtility::broadcastToMap(world::WorldMapInstance& mapInstance, network::aion::AionServerPacket& packet) {
	AION_UNPORTED();
}

// filter lambda com.aionemu.gameserver.utils.PacketSendUtility@L263:46
void PacketSendUtility::broadcastToMap(world::WorldMapInstance& mapInstance, std::shared_ptr<network::aion::AionServerPacket> packet, int32_t delay) {
	AION_UNPORTED();
}

// task lambda com.aionemu.gameserver.utils.PacketSendUtility@L270:17
void PacketSendUtility::broadcastToMap(world::WorldMapInstance& mapInstance, std::shared_ptr<network::aion::AionServerPacket> packet, int32_t delay,
	runtime::PinnedCallback<bool(model::gameobjects::player::Player&)> filter) {
	AION_UNPORTED();
}

// task lambda com.aionemu.gameserver.utils.PacketSendUtility@L277:17
void PacketSendUtility::broadcastToZone(world::zone::ZoneInstance& zone, network::aion::AionServerPacket& packet) {
	AION_UNPORTED();
}

void PacketSendUtility::scheduleOrRun(runtime::PinnedCallback<void()> r, int32_t delay) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::utils
