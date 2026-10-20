#include "aion/gameserver/services/panesterra/ahserion/PanesterraTeam.h"

#include "aion/gameserver/model/Race.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/services/teleport/TeleportService.h"
#include "aion/gameserver/world/World.h"
#include "aion/gameserver/services/panesterra/ahserion/PanesterraFaction.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::services::panesterra::ahserion {

namespace {

/** The origin position the switch of the Java constructor assigns (null for the temples) */
runtime::Ref<world::WorldPosition> originPositionOf(PanesterraFaction faction) {
	switch (faction) {
	case PanesterraFaction::BELUS:
		return world::WorldPosition::create(400020000, 1024.172f, 1063.969f, 1530.3f, int8_t{90});
	case PanesterraFaction::ASPIDA:
		return world::WorldPosition::create(400040000, 1024.172f, 1063.969f, 1530.3f, int8_t{90});
	case PanesterraFaction::ATANATOS:
		return world::WorldPosition::create(110070000, 503.567f, 375.164f, 126.790f, int8_t{30});
	case PanesterraFaction::DISILLON:
		return world::WorldPosition::create(120080000, 429.001f, 250.508f, 93.129f, int8_t{60});
	default:
		return nullptr;
	}
}

/** The start position the switch of the Java constructor assigns */
runtime::Ref<world::WorldPosition> startPositionOf(PanesterraFaction faction) {
	switch (faction) {
	case PanesterraFaction::BELUS:
		return world::WorldPosition::create(400030000, 287.727f, 291.105f, 680.106f, int8_t{15});
	case PanesterraFaction::IVY_TEMPLE:
		return world::WorldPosition::create(400020000, 550.663f, 552.074f, 1484.714f, int8_t{15});
	case PanesterraFaction::HIGHLAND_TEMPLE:
		return world::WorldPosition::create(400020000, 551.551f, 1496.771f, 1484.714f, int8_t{105});
	case PanesterraFaction::ALPINE_TEMPLE:
		return world::WorldPosition::create(400020000, 1494.988f, 1495.968f, 1484.714f, int8_t{72});
	case PanesterraFaction::GRANDWEIR_TEMPLE:
		return world::WorldPosition::create(400020000, 1495.438f, 551.718f, 1484.714f, int8_t{45});
	case PanesterraFaction::ASPIDA:
		return world::WorldPosition::create(400030000, 288.272f, 731.896f, 680.117f, int8_t{105});
	case PanesterraFaction::NOERREN_TEMPLE:
		return world::WorldPosition::create(400040000, 550.663f, 552.074f, 1484.714f, int8_t{15});
	case PanesterraFaction::BOREALIS_TEMPLE:
		return world::WorldPosition::create(400040000, 551.551f, 1496.771f, 1484.714f, int8_t{105});
	case PanesterraFaction::MYRKREN_TEMPLE:
		return world::WorldPosition::create(400040000, 1494.988f, 1495.968f, 1484.714f, int8_t{72});
	case PanesterraFaction::GLUMVEILEN_TEMPLE:
		return world::WorldPosition::create(400040000, 1495.438f, 551.718f, 1484.714f, int8_t{45});
	case PanesterraFaction::ATANATOS:
		return world::WorldPosition::create(400030000, 728.675f, 735.638f, 680.099f, int8_t{75});
	case PanesterraFaction::MEMORIA_TEMPLE:
		return world::WorldPosition::create(400050000, 550.663f, 552.074f, 1484.714f, int8_t{15});
	case PanesterraFaction::SYBILLINE_TEMPLE:
		return world::WorldPosition::create(400050000, 551.551f, 1496.771f, 1484.714f, int8_t{105});
	case PanesterraFaction::AUSTERITY_TEMPLE:
		return world::WorldPosition::create(400050000, 1494.988f, 1495.968f, 1484.714f, int8_t{72});
	case PanesterraFaction::SERENITY_TEMPLE:
		return world::WorldPosition::create(400050000, 1495.438f, 551.718f, 1484.714f, int8_t{45});
	case PanesterraFaction::DISILLON:
		return world::WorldPosition::create(400030000, 730.642f, 293.440f, 680.118f, int8_t{45});
	case PanesterraFaction::NECROLUCE_TEMPLE:
		return world::WorldPosition::create(400060000, 550.663f, 552.074f, 1484.714f, int8_t{15});
	case PanesterraFaction::ESMERAUDUS_TEMPLE:
		return world::WorldPosition::create(400060000, 551.551f, 1496.771f, 1484.714f, int8_t{105});
	case PanesterraFaction::VOLTAIC_TEMPLE:
		return world::WorldPosition::create(400060000, 1494.988f, 1495.968f, 1484.714f, int8_t{72});
	case PanesterraFaction::ILLUMINATUS_TEMPLE:
		return world::WorldPosition::create(400060000, 1495.438f, 551.718f, 1484.714f, int8_t{45});
	default:
		return nullptr;
	}
}

} // namespace

// Java: new WorldPosition(110070000, 503.567f, 375.164f, 126.790f, (byte) 30)
const runtime::Ref<world::WorldPosition>& PanesterraTeam::ELYOS_ORIGIN_POS =
	*new runtime::Ref<world::WorldPosition>(world::WorldPosition::create(110070000, 503.567f, 375.164f, 126.790f, int8_t{30}));

// Java: new WorldPosition(120080000, 429.001f, 250.508f, 93.129f, (byte) 60)
const runtime::Ref<world::WorldPosition>& PanesterraTeam::ASMO_ORIGIN_POS =
	*new runtime::Ref<world::WorldPosition>(world::WorldPosition::create(120080000, 429.001f, 250.508f, 93.129f, int8_t{60}));

PanesterraTeam::PanesterraTeam(PanesterraFaction value)
	: faction(value), originPosition(originPositionOf(value)), startPosition(startPositionOf(value)) {
}

runtime::Ref<PanesterraTeam> PanesterraTeam::create(PanesterraFaction value) {
	return runtime::makeRef<PanesterraTeam>(value);
}

// Java PanesterraTeam.java:65-70
void PanesterraTeam::moveTeamMembersToOriginPosition() {
	forEachMember([this](model::gameobjects::player::Player& player) {
		if (player.getWorldId() == 400030000)
			movePlayerToOriginPosition(player);
	});
}

// Java PanesterraTeam.java:72-78
void PanesterraTeam::forEachMember(const std::function<void(model::gameobjects::player::Player&)>& consumer) {
	for (int32_t playerId : teamMembers.snapshot()) {
		runtime::Ptr<model::gameobjects::player::Player> player = world::World::getInstance().getPlayer(playerId);
		if (player)
			consumer(*player);
	}
}

// Java PanesterraTeam.java:80-91
void PanesterraTeam::movePlayerToOriginPosition(model::gameobjects::player::Player& player) {
	runtime::Ptr<world::WorldPosition> targetPosition;
	switch (faction) {
		case PanesterraFaction::BELUS:
		case PanesterraFaction::ASPIDA:
		case PanesterraFaction::ATANATOS:
		case PanesterraFaction::DISILLON:
			targetPosition = originPosition;
			break;
		default:
			switch (player.getRace()) {
				case model::Race::ELYOS:
					targetPosition = ELYOS_ORIGIN_POS;
					break;
				case model::Race::ASMODIANS:
					targetPosition = ASMO_ORIGIN_POS;
					break;
				default:
					break;
			}
			break;
	}
	if (targetPosition)
		services::teleport::TeleportService::teleportTo(player, *targetPosition);
}

// Java PanesterraTeam.java:93-95
void PanesterraTeam::movePlayerToStartPosition(model::gameobjects::player::Player& player) {
	if (!startPosition) // Java: TeleportService.teleportTo(player, null) dereferences it
		throw runtime::NullPointerException("PanesterraTeam.startPosition of PEACE/BALAUR");
	services::teleport::TeleportService::teleportTo(player, *startPosition);
}

// Java PanesterraTeam.java:97-101
void PanesterraTeam::addTeamMemberIfAbsent(int32_t playerId) {
	if (teamMembers.contains(playerId))
		return;
	teamMembers.add(playerId);
}

// Java PanesterraTeam.java:103-105
bool PanesterraTeam::isTeamMember(int32_t playerId) {
	return teamMembers.contains(playerId);
}

// Java PanesterraTeam.java:107-110, corrected (owner's decision 2026-10-19, P5-12b.md): Java's teamMembers.remove(playerId) with an int is
// List.remove(int index), which takes a member's object id as an index; the C++ removes the id, Java's remove(Integer.valueOf(playerId))
void PanesterraTeam::removeTeamMember(int32_t playerId) {
	if (teamMembers.contains(playerId))
		teamMembers.remove(playerId);
}

// Java PanesterraTeam.java:124-126
int32_t PanesterraTeam::getMemberCount() {
	return teamMembers.size();
}

PanesterraTeam::~PanesterraTeam() = default;

} // namespace aion::gameserver::services::panesterra::ahserion
