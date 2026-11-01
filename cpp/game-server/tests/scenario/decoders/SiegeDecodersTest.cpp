// The siege and world-event decoders (m5i-plan.md G-03) against bodies written field by field from the Java writeImpl of each packet: a
// decoded body gives back its fields, a body one byte short or long is a DecodeError, and the Java constants are checked. No case includes or
// mirrors a C++ serverpackets header.

#include <gtest/gtest.h>

#include <cstdint>
#include <span>
#include <vector>

#include "aion/clientprotocol/decoders/SiegeDecoders.h"
#include "aion/clientprotocol/PacketWriter.h"

#include "NetworkTestSupport.h"

namespace aion::clientprotocol::decoders {
namespace {


/** a body one byte short and one byte long both fail: the decoder consumes the body exactly */
template <class Decode>
void expectExactLength(const std::vector<uint8_t>& body, Decode decode, const char* name) {
	ASSERT_FALSE(body.empty()) << name;
	std::vector<uint8_t> shorter(body.begin(), body.end() - 1);
	EXPECT_THROW(decode(std::span<const uint8_t>(shorter)), DecodeError) << name << ": one byte short";
	std::vector<uint8_t> longer = body;
	longer.push_back(0);
	EXPECT_THROW(decode(std::span<const uint8_t>(longer)), DecodeError) << name << ": one byte long";
}

/** one SM_SIEGE_LOCATION_INFO entry as SM_SIEGE_LOCATION_INFO.java writes it */
void writeLocation(PacketWriter& w, int32_t locId, int32_t legionId, uint8_t race, bool vulnerable, bool canTeleport, uint8_t nextState,
	int32_t timer, int32_t occupied) {
	w.D(locId).D(legionId).C(3).C(1).H(0).C(255).C(10).C(20).C(30).C(race).C(vulnerable ? 2 : 0).C(canTeleport ? 1 : 0).C(nextState).H(0).H(0)
		.D(timer).D(1).D(0).D(occupied);
}

TEST(SiegeDecodersTest, SiegeLocationInfoAtLoginKeepsTheOrder) {
	PacketWriter w;
	w.C(0).H(3);
	writeLocation(w, 1131, 0, 2, false, true, 1, 0, 2);
	writeLocation(w, 1011, 4242, 0, true, false, 0, 0, 0);
	writeLocation(w, 2111, 0, 0, false, true, 0, 900, 0);
	const SiegeLocationInfo info = decodeSiegeLocationInfo(w.data);
	EXPECT_EQ(info.infoType, 0);
	ASSERT_EQ(info.locations.size(), 3u);
	EXPECT_EQ(info.locations[0].locationId, 1131) << "the order written, not sorted";
	EXPECT_EQ(info.locations[1].locationId, 1011);
	EXPECT_EQ(info.locations[1].legionId, 4242);
	EXPECT_TRUE(info.locations[1].vulnerable) << "isVulnerable() ? 2 : 0";
	EXPECT_FALSE(info.locations[1].canTeleport);
	EXPECT_EQ(info.locations[0].raceId, 2);
	EXPECT_EQ(info.locations[0].occupiedCount, 2);
	EXPECT_EQ(info.locations[2].remainingSiegeSeconds, 900) << "the veille/masta timer of 2111 and 3111";
	EXPECT_EQ(info.locations[0].emblemId, 3);
	EXPECT_EQ(info.locations[0].colorB, 30);
	expectExactLength(w.data, [](std::span<const uint8_t> b) { return decodeSiegeLocationInfo(b); }, "SM_SIEGE_LOCATION_INFO");

	PacketWriter disabled;
	disabled.C(0).H(0);
	EXPECT_TRUE(decodeSiegeLocationInfo(disabled.data).locations.empty()) << "sieges disabled: C(0) H(0)";

	PacketWriter badVulnerable;
	badVulnerable.C(1).H(1);
	writeLocation(badVulnerable, 1131, 0, 2, false, true, 1, 0, 0);
	badVulnerable.data[3 + 4 + 4 + 1 + 1 + 2 + 4 + 1] = 1; // header, ids, emblem, H(0), colours, race: the vulnerable byte is 0 or 2
	EXPECT_THROW(decodeSiegeLocationInfo(badVulnerable.data), DecodeError);
}

TEST(SiegeDecodersTest, LocationStateAndFortressInfo) {
	PacketWriter state;
	state.D(1131).C(1);
	EXPECT_EQ(decodeSiegeLocationState(state.data).locationId, 1131);
	EXPECT_EQ(decodeSiegeLocationState(state.data).state, 1);
	expectExactLength(state.data, [](std::span<const uint8_t> b) { return decodeSiegeLocationState(b); }, "SM_SIEGE_LOCATION_STATE");

	PacketWriter info;
	info.D(1131).C(0);
	EXPECT_FALSE(decodeFortressInfo(info.data).teleportStatus);
	expectExactLength(info.data, [](std::span<const uint8_t> b) { return decodeFortressInfo(b); }, "SM_FORTRESS_INFO");
}

TEST(SiegeDecodersTest, InfluenceRatioAndFortressStatus) {
	PacketWriter ratio;
	ratio.D(3600).F(0.4f).F(0.35f).F(0.25f).H(2).D(400010000).F(0.5f).F(0.3f).F(0.2f).D(600090000).F(0.f).F(0.f).F(1.f);
	const InfluenceRatio decoded = decodeInfluenceRatio(ratio.data);
	EXPECT_EQ(decoded.secondsUntilNextFortressState, 3600);
	EXPECT_FLOAT_EQ(decoded.asmodians, 0.35f);
	ASSERT_EQ(decoded.worlds.size(), 2u);
	EXPECT_EQ(decoded.worlds[0].worldId, 400010000);
	EXPECT_FLOAT_EQ(decoded.worlds[1].balaur, 1.f);
	expectExactLength(ratio.data, [](std::span<const uint8_t> b) { return decodeInfluenceRatio(b); }, "SM_INFLUENCE_RATIO");

	PacketWriter status;
	status.C(1).D(60).F(0.5f).F(0.5f).F(0.f).H(0).H(2).D(1131).C(1).D(1011).C(0);
	const FortressStatus fs = decodeFortressStatus(status.data);
	EXPECT_EQ(fs.influence.secondsUntilNextFortressState, 60);
	ASSERT_EQ(fs.fortresses.size(), 2u);
	EXPECT_EQ(fs.fortresses[0].locationId, 1131);
	EXPECT_EQ(fs.fortresses[1].nextState, 0);
	expectExactLength(status.data, [](std::span<const uint8_t> b) { return decodeFortressStatus(b); }, "SM_FORTRESS_STATUS");
	status.data[0] = 0;
	EXPECT_THROW(decodeFortressStatus(status.data), DecodeError) << "writeC(1)";
}

TEST(SiegeDecodersTest, ShieldsAndArtifacts) {
	PacketWriter shield;
	shield.H(2).D(1131).C(1).D(1132).C(0);
	const ShieldEffect effect = decodeShieldEffect(shield.data);
	ASSERT_EQ(effect.locations.size(), 2u);
	EXPECT_TRUE(effect.locations[0].underShield);
	EXPECT_EQ(effect.locations[1].locationId, 1132);
	expectExactLength(shield.data, [](std::span<const uint8_t> b) { return decodeShieldEffect(b); }, "SM_SHIELD_EFFECT");

	PacketWriter artifacts;
	artifacts.H(2).D(1135 * 10 + 1).C(2).D(0).D(1012 * 10 + 1).C(0).D(0);
	const ArtifactInfo3 info = decodeArtifactInfo3(artifacts.data);
	ASSERT_EQ(info.artifacts.size(), 2u);
	EXPECT_EQ(info.artifacts[0].locationId, 1135) << "locationId * 10 + 1";
	EXPECT_EQ(info.artifacts[0].status, 2);
	expectExactLength(artifacts.data, [](std::span<const uint8_t> b) { return decodeArtifactInfo3(b); }, "SM_ABYSS_ARTIFACT_INFO3");
	PacketWriter wrongId;
	wrongId.H(1).D(1135 * 10).C(0).D(0);
	EXPECT_THROW(decodeArtifactInfo3(wrongId.data), DecodeError);
}

TEST(SiegeDecodersTest, RiftAnnounceInAllFiveActions) {
	PacketWriter announce;
	announce.H(1 + 3 * 4).C(0).D(1).D(0).D(1);
	EXPECT_EQ(decodeRiftAnnounce(announce.data).riftValues, (std::vector<int32_t>{1, 0, 1}));
	PacketWriter silentera;
	silentera.H(9).C(1).D(1).D(0);
	EXPECT_EQ(decodeRiftAnnounce(silentera.data).gelkmaros, 1);
	PacketWriter master;
	master.H(35).C(2).D(500001).D(24).D(3600).D(30).D(65).F(1.f).F(2.f).F(3.f).C(4).C(1);
	const RiftAnnounce m = decodeRiftAnnounce(master.data);
	EXPECT_EQ(m.objectId, 500001);
	EXPECT_EQ(m.maxLevel, 65);
	EXPECT_FLOAT_EQ(m.z, 3.f);
	EXPECT_EQ(m.riftType, 4) << "a volatile rift";
	EXPECT_TRUE(m.master);
	PacketWriter slave;
	slave.H(15).C(3).D(500002).D(7).D(1800).C(0).C(0);
	EXPECT_EQ(decodeRiftAnnounce(slave.data).usedEntries, 7);
	PacketWriter despawn;
	despawn.H(5).C(4).D(500001);
	EXPECT_EQ(decodeRiftAnnounce(despawn.data).objectId, 500001);
	for (const PacketWriter* w : {&announce, &silentera, &master, &slave, &despawn})
		expectExactLength(w->data, [](std::span<const uint8_t> b) { return decodeRiftAnnounce(b); }, "SM_RIFT_ANNOUNCE");
	PacketWriter badType;
	badType.H(15).C(3).D(1).D(1).D(1).C(2).C(0);
	EXPECT_THROW(decodeRiftAnnounce(badType.data), DecodeError) << "rift types are 0, 1, 4 and 5";
}

TEST(SiegeDecodersTest, AfterSiegeLocInfoAndNpcAssembler) {
	PacketWriter after;
	after.H(0).C(0);
	EXPECT_NO_THROW(decodeAfterSiegeLocInfo475(after.data));
	expectExactLength(after.data, [](std::span<const uint8_t> b) {
		decodeAfterSiegeLocInfo475(b);
		return 0;
	}, "SM_AFTER_SIEGE_LOCINFO_475");

	PacketWriter assembler;
	assembler.D(2).D(7).D(5001).D(276793).D(100).Q(60000).D(7).D(5002).D(276794).D(101).Q(60000);
	const NpcAssembler a = decodeNpcAssembler(assembler.data);
	ASSERT_EQ(a.parts.size(), 2u);
	EXPECT_EQ(a.parts[1].npcId, 276794);
	EXPECT_EQ(a.parts[0].timeOnMap, 60000);
	expectExactLength(assembler.data, [](std::span<const uint8_t> b) { return decodeNpcAssembler(b); }, "SM_NPC_ASSEMBLER");
	PacketWriter none;
	none.D(0);
	EXPECT_TRUE(decodeNpcAssembler(none.data).parts.empty()) << "no assembled npc: writeD(0)";
}

} // namespace
} // namespace aion::clientprotocol::decoders
