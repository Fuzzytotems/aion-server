#pragma once

#include <concepts>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/sched/PinnedCallback.h"
#include "aion/gameserver/ai/event/fwd.h"
#include "aion/gameserver/model/fwd.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/team/legion/fwd.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"
#include "aion/gameserver/utils/fwd.h"
#include "aion/gameserver/world/fwd.h"
#include "aion/gameserver/world/zone/fwd.h"

namespace aion::gameserver::utils {

/**
 * This class contains static methods, which are utility methods, all of them are interacting with player.
 * <p>
 * Hub header (docs/design/hub-headers.md §12, runtime-architecture.md §8.2 and §14.2(e)). A static-only class (fieldmap K5).
 * <ul>
 * <li>Packets are stack temporaries: each non-template overload takes `AionServerPacket&` and has a forwarding template for temporaries
 * (`PacketSendUtility::sendPacket(player, SM_X(...))`). The constraint and the conversion are checked at the call site, so a forward declaration
 * of AionServerPacket suffices here. A broadcast serializes a SHARED packet once and PER_RECIPIENT packets per recipient.</li>
 * <li>Overloads with a `delay` may run later on the scheduled pool (Java scheduleOrRun), so they cannot borrow the packet: the non-template
 * overload takes `std::shared_ptr<AionServerPacket>` and the template copies (or moves) the packet into one; their filters are stored, hence
 * `runtime::PinnedCallback`.</li>
 * <li>`Object... params` that are only formatted into SM_SYSTEM_MESSAGE are variadic templates converting each argument to its Java string
 * (strings as they are, everything else with `SM_SYSTEM_MESSAGE::toJavaString`), forwarding to the `std::vector<std::string>` overload. Call
 * sites that pass parameters include SM_SYSTEM_MESSAGE.h (the conversion is resolved when the template is instantiated).</li>
 * </ul>
 *
 * @author Luno, Neon
 */
class PacketSendUtility {
private:
	/** Java String.valueOf / toString() of one `Object...` message parameter (resolved at the call site, see the class comment). */
	template <class SysMsg, class Param>
	static std::string toMessageParam(Param&& param) {
		if constexpr (std::is_convertible_v<Param, std::string_view>)
			return std::string(std::string_view(param));
		else
			return SysMsg::toJavaString(std::forward<Param>(param));
	}

public:
	/** Sends a message to a player (ChatType.GOLDEN_YELLOW) */
	static void sendMessage(model::gameobjects::player::Player& player, std::string_view msg);

	static void sendMessage(model::gameobjects::player::Player& player, std::string_view msg, model::ChatType chatType);

	/** Java sendMonologue(Player player, int msgId, Object... params) */
	template <class... Params, class SysMsg = network::aion::serverpackets::SM_SYSTEM_MESSAGE>
	static void sendMonologue(model::gameobjects::player::Player& player, int32_t msgId, Params&&... params) {
		sendMonologue(player, msgId, std::vector<std::string>{toMessageParam<SysMsg>(std::forward<Params>(params))...});
	}

	/** Sends a system message with the player as sender (ChatType.NORMAL); params are already converted to their Java strings */
	static void sendMonologue(model::gameobjects::player::Player& player, int32_t msgId, std::vector<std::string> params);

	/** Java sendMessage(Player player, Npc npc, int msgId, Object... params) */
	template <class... Params, class SysMsg = network::aion::serverpackets::SM_SYSTEM_MESSAGE>
	static void sendMessage(model::gameobjects::player::Player& player, model::gameobjects::Npc& npc, int32_t msgId, Params&&... params) {
		sendMessage(player, npc, msgId, std::vector<std::string>{toMessageParam<SysMsg>(std::forward<Params>(params))...});
	}

	/** Sends a system message with the npc as sender (ChatType.NPC); params are already converted to their Java strings */
	static void sendMessage(model::gameobjects::player::Player& player, model::gameobjects::Npc& npc, int32_t msgId, std::vector<std::string> params);

	/** Broadcasts an NPC message immediately (does nothing if npc is null) */
	static void broadcastMessage(runtime::Ptr<model::gameobjects::Npc> npc, int32_t msgId);

	/** Java broadcastMessage(Npc npc, int msgId, int delay, Object... msgParams) */
	template <class... Params, class SysMsg = network::aion::serverpackets::SM_SYSTEM_MESSAGE>
	static void broadcastMessage(runtime::Ptr<model::gameobjects::Npc> npc, int32_t msgId, int32_t delay, Params&&... msgParams) {
		broadcastMessage(npc, msgId, delay, std::vector<std::string>{toMessageParam<SysMsg>(std::forward<Params>(msgParams))...});
	}

	/**
	 * Broadcasts an NPC message to players within 50m after the delay, if the npc is still spawned then (does nothing if npc is null); msgParams
	 * are already converted to their Java strings.
	 */
	static void broadcastMessage(runtime::Ptr<model::gameobjects::Npc> npc, int32_t msgId, int32_t delay, std::vector<std::string> msgParams);

	/** Send packet to this player (if online) */
	static void sendPacket(model::gameobjects::player::Player& player, network::aion::AionServerPacket& packet);

	template <std::derived_from<network::aion::AionServerPacket> P>
	static void sendPacket(model::gameobjects::player::Player& player, P&& packet) {
		sendPacket(player, static_cast<network::aion::AionServerPacket&>(packet));
	}

	/** Broadcast packet to all visible players. */
	static void broadcastPacket(model::gameobjects::player::Player& player, network::aion::AionServerPacket& packet, bool toSelf);

	template <std::derived_from<network::aion::AionServerPacket> P>
	static void broadcastPacket(model::gameobjects::player::Player& player, P&& packet, bool toSelf) {
		broadcastPacket(player, static_cast<network::aion::AionServerPacket&>(packet), toSelf);
	}

	/** Broadcast packet to all visible players. */
	static void broadcastPacket(model::gameobjects::VisibleObject& object, network::aion::AionServerPacket& packet);

	template <std::derived_from<network::aion::AionServerPacket> P>
	static void broadcastPacket(model::gameobjects::VisibleObject& object, P&& packet) {
		broadcastPacket(object, static_cast<network::aion::AionServerPacket&>(packet));
	}

	/** Broadcasts packet to all visible players matching a filter */
	static void broadcastPacket(model::gameobjects::VisibleObject& object, network::aion::AionServerPacket& packet,
		const std::function<bool(model::gameobjects::player::Player&)>& filter);

	template <std::derived_from<network::aion::AionServerPacket> P>
	static void broadcastPacket(model::gameobjects::VisibleObject& object, P&& packet,
		const std::function<bool(model::gameobjects::player::Player&)>& filter) {
		broadcastPacket(object, static_cast<network::aion::AionServerPacket&>(packet), filter);
	}

	/** Broadcast packet to all visible players and the object itself, if it is a player. */
	static void broadcastPacketAndReceive(model::gameobjects::VisibleObject& visibleObject, network::aion::AionServerPacket& packet);

	template <std::derived_from<network::aion::AionServerPacket> P>
	static void broadcastPacketAndReceive(model::gameobjects::VisibleObject& visibleObject, P&& packet) {
		broadcastPacketAndReceive(visibleObject, static_cast<network::aion::AionServerPacket&>(packet));
	}

	/**
	 * Broadcast packet to all visible players and the creature itself, if it is a player, and notifies visible npcs with the AI event.
	 *
	 * @param et
	 *          absent (Java null): no AI event
	 */
	static void broadcastPacketAndReceive(model::gameobjects::Creature& creature, network::aion::AionServerPacket& packet,
		std::optional<ai::event::AIEventType> et);

	template <std::derived_from<network::aion::AionServerPacket> P>
	static void broadcastPacketAndReceive(model::gameobjects::Creature& creature, P&& packet, std::optional<ai::event::AIEventType> et) {
		broadcastPacketAndReceive(creature, static_cast<network::aion::AionServerPacket&>(packet), et);
	}

	/**
	 * Broadcast packet to all visible players and notifies visible npcs with the AI event.
	 *
	 * @param et
	 *          absent (Java null): no AI event
	 */
	static void broadcastPacketAndAIEvent(model::gameobjects::Creature& creature, network::aion::AionServerPacket& packet,
		std::optional<ai::event::AIEventType> et);

	template <std::derived_from<network::aion::AionServerPacket> P>
	static void broadcastPacketAndAIEvent(model::gameobjects::Creature& creature, P&& packet, std::optional<ai::event::AIEventType> et) {
		broadcastPacketAndAIEvent(creature, static_cast<network::aion::AionServerPacket&>(packet), et);
	}

	/** Broadcasts packet to all visible players matching a filter (and to the object itself if toSelf and it is a player) */
	static void broadcastPacket(model::gameobjects::VisibleObject& object, network::aion::AionServerPacket& packet, bool toSelf,
		const std::function<bool(model::gameobjects::player::Player&)>& filter);

	template <std::derived_from<network::aion::AionServerPacket> P>
	static void broadcastPacket(model::gameobjects::VisibleObject& object, P&& packet, bool toSelf,
		const std::function<bool(model::gameobjects::player::Player&)>& filter) {
		broadcastPacket(object, static_cast<network::aion::AionServerPacket&>(packet), toSelf, filter);
	}

	/** Broadcasts packet to all players of the world */
	static void broadcastToWorld(network::aion::AionServerPacket& packet);

	template <std::derived_from<network::aion::AionServerPacket> P>
	static void broadcastToWorld(P&& packet) {
		broadcastToWorld(static_cast<network::aion::AionServerPacket&>(packet));
	}

	/** Broadcasts packet to all players of the world matching a filter */
	static void broadcastToWorld(network::aion::AionServerPacket& packet, const std::function<bool(model::gameobjects::player::Player&)>& filter);

	template <std::derived_from<network::aion::AionServerPacket> P>
	static void broadcastToWorld(P&& packet, const std::function<bool(model::gameobjects::player::Player&)>& filter) {
		broadcastToWorld(static_cast<network::aion::AionServerPacket&>(packet), filter);
	}

	/** Broadcast packet to all online legion members. */
	static void broadcastToLegion(model::team::legion::Legion& legion, network::aion::AionServerPacket& packet);

	template <std::derived_from<network::aion::AionServerPacket> P>
	static void broadcastToLegion(model::team::legion::Legion& legion, P&& packet) {
		broadcastToLegion(legion, static_cast<network::aion::AionServerPacket&>(packet));
	}

	/** Broadcast packet to all online legion members except the player with playerObjId. */
	static void broadcastToLegion(model::team::legion::Legion& legion, network::aion::AionServerPacket& packet, int32_t playerObjId);

	template <std::derived_from<network::aion::AionServerPacket> P>
	static void broadcastToLegion(model::team::legion::Legion& legion, P&& packet, int32_t playerObjId) {
		broadcastToLegion(legion, static_cast<network::aion::AionServerPacket&>(packet), playerObjId);
	}

	/** Broadcasts the packet to all players who can see the object. */
	static void broadcastToSightedPlayers(model::gameobjects::VisibleObject& object, network::aion::AionServerPacket& packet);

	template <std::derived_from<network::aion::AionServerPacket> P>
	static void broadcastToSightedPlayers(model::gameobjects::VisibleObject& object, P&& packet) {
		broadcastToSightedPlayers(object, static_cast<network::aion::AionServerPacket&>(packet));
	}

	/** Broadcasts the packet to all players who can see the object (and to the object itself if toSelf and it is a player). */
	static void broadcastToSightedPlayers(model::gameobjects::VisibleObject& object, network::aion::AionServerPacket& packet, bool toSelf);

	template <std::derived_from<network::aion::AionServerPacket> P>
	static void broadcastToSightedPlayers(model::gameobjects::VisibleObject& object, P&& packet, bool toSelf) {
		broadcastToSightedPlayers(object, static_cast<network::aion::AionServerPacket&>(packet), toSelf);
	}

	/** Sends SM_SYSTEM_MESSAGE(msgId) to all players of the object's map instance except the object. */
	static void broadcastToMap(model::gameobjects::VisibleObject& object, int32_t msgId);

	/** Sends SM_SYSTEM_MESSAGE(msgId) to all players of the object's map instance except the object, after delay ms. */
	static void broadcastToMap(model::gameobjects::VisibleObject& object, int32_t msgId, int32_t delay);

	/** Sends the packet to all players of the object's map instance except the object. */
	static void broadcastToMap(model::gameobjects::VisibleObject& object, network::aion::AionServerPacket& packet);

	template <std::derived_from<network::aion::AionServerPacket> P>
	static void broadcastToMap(model::gameobjects::VisibleObject& object, P&& packet) {
		broadcastToMap(object, static_cast<network::aion::AionServerPacket&>(packet));
	}

	/** Sends the packet to all players of the object's map instance except the object, after delay ms. */
	static void broadcastToMap(model::gameobjects::VisibleObject& object, std::shared_ptr<network::aion::AionServerPacket> packet, int32_t delay);

	template <class P>
		requires std::derived_from<std::remove_cvref_t<P>, network::aion::AionServerPacket>
	static void broadcastToMap(model::gameobjects::VisibleObject& object, P&& packet, int32_t delay) {
		broadcastToMap(object, std::shared_ptr<network::aion::AionServerPacket>(std::make_shared<std::remove_cvref_t<P>>(std::forward<P>(packet))),
			delay);
	}

	/** Sends the packet to all players of the object's map instance matching the filter, after delay ms. */
	static void broadcastToMap(model::gameobjects::VisibleObject& object, std::shared_ptr<network::aion::AionServerPacket> packet, int32_t delay,
		runtime::PinnedCallback<bool(model::gameobjects::player::Player&)> filter);

	template <class P>
		requires std::derived_from<std::remove_cvref_t<P>, network::aion::AionServerPacket>
	static void broadcastToMap(model::gameobjects::VisibleObject& object, P&& packet, int32_t delay,
		runtime::PinnedCallback<bool(model::gameobjects::player::Player&)> filter) {
		broadcastToMap(object, std::shared_ptr<network::aion::AionServerPacket>(std::make_shared<std::remove_cvref_t<P>>(std::forward<P>(packet))), delay,
			std::move(filter));
	}

	/** Sends the packet to all players of the map instance. */
	static void broadcastToMap(world::WorldMapInstance& mapInstance, network::aion::AionServerPacket& packet);

	template <std::derived_from<network::aion::AionServerPacket> P>
	static void broadcastToMap(world::WorldMapInstance& mapInstance, P&& packet) {
		broadcastToMap(mapInstance, static_cast<network::aion::AionServerPacket&>(packet));
	}

	/** Sends the packet to all players of the map instance after delay ms. */
	static void broadcastToMap(world::WorldMapInstance& mapInstance, std::shared_ptr<network::aion::AionServerPacket> packet, int32_t delay);

	template <class P>
		requires std::derived_from<std::remove_cvref_t<P>, network::aion::AionServerPacket>
	static void broadcastToMap(world::WorldMapInstance& mapInstance, P&& packet, int32_t delay) {
		broadcastToMap(mapInstance, std::shared_ptr<network::aion::AionServerPacket>(std::make_shared<std::remove_cvref_t<P>>(std::forward<P>(packet))),
			delay);
	}

	/** Sends the packet to all players of the map instance matching the filter, after delay ms. */
	static void broadcastToMap(world::WorldMapInstance& mapInstance, std::shared_ptr<network::aion::AionServerPacket> packet, int32_t delay,
		runtime::PinnedCallback<bool(model::gameobjects::player::Player&)> filter);

	template <class P>
		requires std::derived_from<std::remove_cvref_t<P>, network::aion::AionServerPacket>
	static void broadcastToMap(world::WorldMapInstance& mapInstance, P&& packet, int32_t delay,
		runtime::PinnedCallback<bool(model::gameobjects::player::Player&)> filter) {
		broadcastToMap(mapInstance, std::shared_ptr<network::aion::AionServerPacket>(std::make_shared<std::remove_cvref_t<P>>(std::forward<P>(packet))),
			delay, std::move(filter));
	}

	/** Sends the packet to all players inside the zone. */
	static void broadcastToZone(world::zone::ZoneInstance& zone, network::aion::AionServerPacket& packet);

	template <std::derived_from<network::aion::AionServerPacket> P>
	static void broadcastToZone(world::zone::ZoneInstance& zone, P&& packet) {
		broadcastToZone(zone, static_cast<network::aion::AionServerPacket&>(packet));
	}

private:
	/** Runs r now if delay <= 0, otherwise schedules it on the scheduled pool. */
	static void scheduleOrRun(runtime::PinnedCallback<void()> r, int32_t delay);

	/**
	 * C++ only: the body of broadcastToMap(WorldMapInstance, AionServerPacket, int delay, Predicate) that scheduleOrRun runs inline for a delay of
	 * 0, so the delay-free overloads can pass their borrowed packet and synchronous filter without a copy.
	 */
	static void sendToMapPlayers(world::WorldMapInstance& mapInstance, network::aion::AionServerPacket& packet,
		const std::function<bool(model::gameobjects::player::Player&)>& filter);
};

} // namespace aion::gameserver::utils
