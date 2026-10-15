// The M5j stage-2 pet and kisk decoders (m5j-plan.md §18.3 CP5, H-21) against bodies written out field by field in the Java writeImpl order
// (m5a-plan.md D9): every case also relies on the decoders' exact-consumption check. No case includes or consults a C++ serverpackets header.

#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

#include "decoders/PetKiskDecoders.h"

#include "NetworkTestSupport.h"

namespace aion::gameserver::scenario::decoders {
namespace {

using network::test::PacketWriter;

/** SM_PET.writeAppearance: writeH(1), writeC(0) x 3, writeD(decoration), writeD(0) x 2 */
PacketWriter& appearance(PacketWriter& w, int32_t decoration) {
	return w.H(1).C(0).C(0).C(0).D(decoration).D(0).D(0);
}

/** SM_PET.writePetData of a pet with the FOOD specialty only: one NONE filler */
PacketWriter& foodPet(PacketWriter& w, int32_t objectId) {
	w.S("Kitty").D(900001).D(objectId).D(700).D(0).D(0).D(1700000000).D(0);
	w.C(1).C(8).D(5).D(0); // FOOD: writeC(id), writeC(8), the feed progress, the refeed delay
	w.H(6);                // writeH(NONE)
	return appearance(w, 3);
}

/** SM_PET.java:154-171: LOAD_PETS, ADOPT and SURRENDER */
TEST(PetKiskDecodersTest, PetListsAdoptionAndSurrender) {
	PacketWriter list;
	list.H(0).C(0).H(2);
	foodPet(list, 41);
	list.S("Rex").D(900002).D(42).D(700).D(0).D(0).D(1700000001).D(3600);
	list.C(3).C(1).C(0); // LOOT: writeC(id), writeC(1), writeC(0)
	list.C(0).C(0);      // WAREHOUSE: writeC(id), writeC(0)
	appearance(list, 0);
	const Pet loaded = decodePet(list.data);
	EXPECT_EQ(loaded.action, PET_ACTION_LOAD_PETS);
	ASSERT_EQ(loaded.pets.size(), 2u);
	EXPECT_EQ(loaded.pets[0].name, "Kitty");
	EXPECT_EQ(loaded.pets[0].masterObjectId, 700);
	ASSERT_EQ(loaded.pets[0].functions.size(), 1u);
	EXPECT_EQ(loaded.pets[0].functions[0].id, PET_FUNCTION_FOOD);
	EXPECT_EQ(loaded.pets[0].decoration, 3);
	EXPECT_EQ(loaded.pets[1].secondsUntilExpiration, 3600);
	ASSERT_EQ(loaded.pets[1].functions.size(), 2u);
	EXPECT_EQ(loaded.pets[1].functions[1].id, PET_FUNCTION_WAREHOUSE);
	EXPECT_TRUE(decodePet(PacketWriter().H(0).C(0).H(0).data).pets.empty());
	EXPECT_THROW(decodePet(PacketWriter().H(0).C(1).H(0).data), DecodeError) << "the literal writeC(0)";

	PacketWriter adopt;
	adopt.H(1);
	foodPet(adopt, 43);
	const Pet adopted = decodePet(adopt.data);
	ASSERT_TRUE(adopted.adopted);
	EXPECT_EQ(adopted.adopted->objectId, 43);
	PacketWriter badFiller;
	badFiller.H(1).S("Kitty").D(900001).D(43).D(700).D(0).D(0).D(0).D(0).H(6).H(6).H(2);
	EXPECT_THROW(decodePet(badFiller.data), DecodeError) << "APPEARANCE is writeH(1)";

	const Pet surrendered = decodePet(PacketWriter().H(2).D(900001).D(43).D(0).D(0).data);
	EXPECT_EQ(surrendered.surrenderedTemplateId, 900001);
	EXPECT_EQ(surrendered.petObjectId, 43);
	EXPECT_THROW(decodePet(PacketWriter().H(2).D(900001).D(43).D(1).D(0).data), DecodeError);
}

/** SM_PET.java:172-191: SPAWN and DISMISS */
TEST(PetKiskDecodersTest, PetSpawnAndDismiss) {
	PacketWriter w;
	w.H(3).S("Kitty").D(900001).D(43).F(1.5f).F(2.5f).F(3.5f).F(4.0f).F(5.0f).F(6.0f).C(30).D(700);
	appearance(w, 0);
	const Pet spawned = decodePet(w.data);
	ASSERT_TRUE(spawned.spawn);
	EXPECT_EQ(spawned.spawn->name, "Kitty");
	EXPECT_EQ(spawned.spawn->objectId, 43);
	EXPECT_EQ(spawned.spawn->y, 2.5f);
	EXPECT_EQ(spawned.spawn->targetZ, 6.0f);
	EXPECT_EQ(spawned.spawn->heading, 30);
	EXPECT_EQ(spawned.spawn->masterObjectId, 700);
	const Pet dismissed = decodePet(PacketWriter().H(4).D(43).C(2).data);
	EXPECT_EQ(dismissed.petObjectId, 43);
	EXPECT_EQ(dismissed.animationId, 2);
	EXPECT_THROW(decodePet(PacketWriter().H(4).D(43).data), DecodeError);
}

/** SM_PET.java:192-311: FOOD, RENAME, MOOD and SPECIAL_FUNCTION */
TEST(PetKiskDecodersTest, PetFoodMoodAndFunctions) {
	const Pet eat = decodePet(PacketWriter().H(9).H(1).C(1).C(1).D(7).D(0).D(55).D(2).data);
	EXPECT_EQ(eat.subType, 1);
	EXPECT_EQ(eat.value1, 7);
	EXPECT_EQ(eat.values, (std::vector<int32_t>{0, 55, 2}));
	EXPECT_EQ(decodePet(PacketWriter().H(9).H(1).C(1).C(2).D(7).D(0).D(55).D(1).C(0).data).values.size(), 3u);
	EXPECT_EQ(decodePet(PacketWriter().H(9).H(1).C(1).C(4).D(7).D(30).data).values, (std::vector<int32_t>{30}));
	EXPECT_THROW(decodePet(PacketWriter().H(9).H(2).C(1).C(4).D(7).D(30).data), DecodeError) << "the literal writeH(1)";
	const Pet renamed = decodePet(PacketWriter().H(10).D(43).S("Tom").data);
	EXPECT_EQ(renamed.newName, "Tom");
	const Pet periodic = decodePet(PacketWriter().H(12).C(4).D(100).D(20).D(30).data);
	EXPECT_EQ(periodic.subType, 4);
	EXPECT_EQ(periodic.value1, 100);
	EXPECT_EQ(periodic.values, (std::vector<int32_t>{20, 30}));
	EXPECT_EQ(decodePet(PacketWriter().H(12).C(2).D(0).D(50).D(9).data).values, (std::vector<int32_t>{9}));
	EXPECT_THROW(decodePet(PacketWriter().H(12).C(2).D(1).D(50).D(9).data), DecodeError);
	const Pet loot = decodePet(PacketWriter().H(13).C(3).C(0).C(1).data);
	EXPECT_EQ(loot.values, (std::vector<int32_t>{0, 1}));
	EXPECT_EQ(decodePet(PacketWriter().H(13).C(3).C(2).D(9001).data).values, (std::vector<int32_t>{2, 9001}));
	EXPECT_EQ(decodePet(PacketWriter().H(13).C(2).C(0).D(1).D(2).data).values, (std::vector<int32_t>{0, 1, 2}));
	EXPECT_EQ(decodePet(PacketWriter().H(6).data).action, 6) << "TALK_WITH_MERCHANT: the action alone";
}

/** SM_KISK_UPDATE.java writeImpl: eight ints */
TEST(PetKiskDecodersTest, KiskUpdate) {
	const KiskUpdate update = decodeKiskUpdate(PacketWriter().D(5000).D(700).D(1).D(1).D(6).D(10).D(10).D(3599).data);
	EXPECT_EQ(update.kiskObjectId, 5000);
	EXPECT_EQ(update.creatorId, 700);
	EXPECT_EQ(update.currentMembers, 1);
	EXPECT_EQ(update.maxMembers, 6);
	EXPECT_EQ(update.remainingLifetimeSeconds, 3599);
	EXPECT_THROW(decodeKiskUpdate(PacketWriter().D(1).data), DecodeError);
}

} // namespace
} // namespace aion::gameserver::scenario::decoders
