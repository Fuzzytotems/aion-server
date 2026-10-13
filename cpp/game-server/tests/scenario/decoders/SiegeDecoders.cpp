#include "decoders/SiegeDecoders.h"

#include <string>

namespace aion::gameserver::scenario::decoders {

SiegeLocationInfo decodeSiegeLocationInfo(std::span<const uint8_t> body) {
	BodyReader reader(body, "SM_SIEGE_LOCATION_INFO");
	SiegeLocationInfo info;
	info.infoType = reader.C(); // writeC(infoType), or writeC(0) with sieges disabled
	const uint16_t count = reader.H();
	for (uint16_t i = 0; i < count; i++) {
		SiegeLocationEntry loc;
		loc.locationId = reader.D();
		loc.legionId = reader.D();
		loc.emblemId = reader.C();
		loc.emblemType = reader.C();
		reader.expectH(0, "writeH(0) after the emblem type");
		loc.colorA = reader.C();
		loc.colorR = reader.C();
		loc.colorG = reader.C();
		loc.colorB = reader.C();
		loc.raceId = reader.C();
		const uint8_t vulnerable = reader.C(); // isVulnerable() ? 2 : 0
		if (vulnerable != 0 && vulnerable != 2)
			reader.fail("vulnerable byte " + std::to_string(vulnerable) + " is neither 0 nor 2");
		loc.vulnerable = vulnerable == 2;
		const uint8_t teleport = reader.C();
		if (teleport > 1)
			reader.fail("canTeleport byte " + std::to_string(teleport));
		loc.canTeleport = teleport == 1;
		loc.nextState = reader.C();
		reader.expectH(0, "writeH(0) unk");
		reader.expectH(0, "writeH(0)");
		loc.remainingSiegeSeconds = reader.D();
		loc.serverId = reader.D();
		reader.expectD(0, "writeD(0) unk 4.7");
		loc.occupiedCount = reader.D();
		info.locations.push_back(loc);
	}
	reader.expectFullyConsumed();
	return info;
}

SiegeLocationState decodeSiegeLocationState(std::span<const uint8_t> body) {
	BodyReader reader(body, "SM_SIEGE_LOCATION_STATE");
	SiegeLocationState state;
	state.locationId = reader.D();
	state.state = reader.C();
	reader.expectFullyConsumed();
	return state;
}

FortressInfo decodeFortressInfo(std::span<const uint8_t> body) {
	BodyReader reader(body, "SM_FORTRESS_INFO");
	FortressInfo info;
	info.locationId = reader.D();
	const uint8_t status = reader.C();
	if (status > 1)
		reader.fail("teleportStatus byte " + std::to_string(status));
	info.teleportStatus = status == 1;
	reader.expectFullyConsumed();
	return info;
}

namespace {
InfluenceRatio readInfluence(BodyReader& reader) {
	InfluenceRatio ratio;
	ratio.secondsUntilNextFortressState = reader.D();
	ratio.elyos = reader.F();
	ratio.asmodians = reader.F();
	ratio.balaur = reader.F();
	const uint16_t count = reader.H();
	for (uint16_t i = 0; i < count; i++) {
		WorldInfluence world;
		world.worldId = reader.D();
		world.elyos = reader.F();
		world.asmodians = reader.F();
		world.balaur = reader.F();
		ratio.worlds.push_back(world);
	}
	return ratio;
}
} // namespace

InfluenceRatio decodeInfluenceRatio(std::span<const uint8_t> body) {
	BodyReader reader(body, "SM_INFLUENCE_RATIO");
	InfluenceRatio ratio = readInfluence(reader);
	reader.expectFullyConsumed();
	return ratio;
}

FortressStatus decodeFortressStatus(std::span<const uint8_t> body) {
	BodyReader reader(body, "SM_FORTRESS_STATUS");
	reader.expectC(1, "writeC(1)");
	FortressStatus status;
	status.influence = readInfluence(reader);
	const uint16_t count = reader.H();
	for (uint16_t i = 0; i < count; i++) {
		FortressStatus::Fortress fortress;
		fortress.locationId = reader.D();
		fortress.nextState = reader.C();
		status.fortresses.push_back(fortress);
	}
	reader.expectFullyConsumed();
	return status;
}

ShieldEffect decodeShieldEffect(std::span<const uint8_t> body) {
	BodyReader reader(body, "SM_SHIELD_EFFECT");
	ShieldEffect effect;
	const uint16_t count = reader.H();
	for (uint16_t i = 0; i < count; i++) {
		ShieldEffect::Location loc;
		loc.locationId = reader.D();
		const uint8_t shield = reader.C();
		if (shield > 1)
			reader.fail("underShield byte " + std::to_string(shield));
		loc.underShield = shield == 1;
		effect.locations.push_back(loc);
	}
	reader.expectFullyConsumed();
	return effect;
}

ArtifactInfo3 decodeArtifactInfo3(std::span<const uint8_t> body) {
	BodyReader reader(body, "SM_ABYSS_ARTIFACT_INFO3");
	ArtifactInfo3 info;
	const uint16_t count = reader.H();
	for (uint16_t i = 0; i < count; i++) {
		const int32_t encoded = reader.D(); // writeD(artifact.getLocationId() * 10 + 1)
		if (encoded % 10 != 1)
			reader.fail("artifact id " + std::to_string(encoded) + " is not locationId * 10 + 1");
		ArtifactInfo3::Artifact artifact;
		artifact.locationId = encoded / 10;
		artifact.status = reader.C();
		reader.expectD(0, "writeD(0)");
		info.artifacts.push_back(artifact);
	}
	reader.expectFullyConsumed();
	return info;
}

RiftAnnounce decodeRiftAnnounce(std::span<const uint8_t> body) {
	BodyReader reader(body, "SM_RIFT_ANNOUNCE");
	RiftAnnounce rift;
	const uint16_t length = reader.H(); // writeH(following byte length)
	if (length != reader.remaining())
		reader.fail("following byte length " + std::to_string(length) + " but " + std::to_string(reader.remaining()) + " bytes follow");
	rift.actionId = reader.C();
	switch (rift.actionId) {
		case 0:
			if ((length - 1) % 4 != 0)
				reader.fail("announce length " + std::to_string(length) + " is not 1 + 4n");
			for (size_t i = 0; i < static_cast<size_t>(length - 1) / 4; i++)
				rift.riftValues.push_back(reader.D());
			break;
		case 1:
			if (length != 9)
				reader.fail("silentera length " + std::to_string(length));
			rift.gelkmaros = reader.D();
			rift.inggison = reader.D();
			break;
		case 2:
			if (length != 35)
				reader.fail("master length " + std::to_string(length));
			rift.objectId = reader.D();
			rift.maxEntries = reader.D();
			rift.remainTime = reader.D();
			rift.minLevel = reader.D();
			rift.maxLevel = reader.D();
			rift.x = reader.F();
			rift.y = reader.F();
			rift.z = reader.F();
			rift.riftType = reader.C();
			rift.master = reader.C() == 1;
			break;
		case 3:
			if (length != 15)
				reader.fail("slave length " + std::to_string(length));
			rift.objectId = reader.D();
			rift.usedEntries = reader.D();
			rift.remainTime = reader.D();
			rift.riftType = reader.C();
			reader.expectC(0, "writeC(0) unk");
			break;
		case 4:
			if (length != 5)
				reader.fail("despawn length " + std::to_string(length));
			rift.objectId = reader.D();
			break;
		default:
			reader.fail("action " + std::to_string(rift.actionId));
	}
	if (rift.actionId == 2 || rift.actionId == 3)
		if (rift.riftType != 0 && rift.riftType != 1 && rift.riftType != 4 && rift.riftType != 5)
			reader.fail("rift type " + std::to_string(rift.riftType));
	reader.expectFullyConsumed();
	return rift;
}

void decodeAfterSiegeLocInfo475(std::span<const uint8_t> body) {
	BodyReader reader(body, "SM_AFTER_SIEGE_LOCINFO_475");
	reader.expectH(0, "writeH(0)");
	reader.expectC(0, "writeC(0)");
	reader.expectFullyConsumed();
}

NpcAssembler decodeNpcAssembler(std::span<const uint8_t> body) {
	BodyReader reader(body, "SM_NPC_ASSEMBLER");
	NpcAssembler assembler;
	const int32_t count = reader.D();
	if (count < 0)
		reader.fail("negative part count");
	for (int32_t i = 0; i < count; i++) {
		NpcAssembler::Part part;
		part.routeId = reader.D();
		part.objectId = reader.D();
		part.npcId = reader.D();
		part.staticId = reader.D();
		part.timeOnMap = reader.Q();
		assembler.parts.push_back(part);
	}
	reader.expectFullyConsumed();
	return assembler;
}

} // namespace aion::gameserver::scenario::decoders
