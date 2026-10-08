// P5-08 toy pets (m5j-plan.md §18.3 stage 2 CP1, item E-01) and their client packet CM_PET (P5-16): adoption and surrender, renaming, the
// mood arms, auto loot and auto sell, the feeding cancel and refeed answers, and the doping guard. Real Players of the party fixture
// (tests/team/P5-10b, by relative path, as SocialDuelServiceTest); a pet is a Pet object of the test pets (PetTestSupport.h) handed to its
// master with setPet, as VisibleObjectSpawner.spawnPet does after bringIntoWorld. The pet DAO has no database here: PlayerPetsDAO logs and
// carries on (its own try/catch), the in-memory lists are what the cases read.
//
// Expectations are derived by hand from PetAdoptionService.java:57-98, PetService.java:53-246, PetMoodService.java:20-80 and CM_PET.java.

#include "../team/P5-10b/TeamTestSupport.h"

#include "PetTestSupport.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <regex>
#include <string>
#include <vector>

#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/configs/main/NameConfig.h"
#include "aion/gameserver/controllers/PetController.h"
#include "aion/gameserver/model/EmotionType.h"
#include "aion/gameserver/model/gameobjects/Pet.h"
#include "aion/gameserver/model/gameobjects/PetAction.h"
#include "aion/gameserver/model/gameobjects/PetSpecialFunction.h"
#include "aion/gameserver/model/gameobjects/player/PetCommonData.h"
#include "aion/gameserver/model/gameobjects/player/PetList.h"
#include "aion/gameserver/model/templates/pet/PetTemplate.h"
#include "aion/gameserver/network/aion/clientpackets/CM_PET.h"
#include "aion/gameserver/network/aion/clientpackets/CM_PET_EMOTE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_EMOTION.h"
#include "aion/gameserver/network/aion/serverpackets/SM_PET.h"
#include "aion/gameserver/services/toypet/PetAdoptionService.h"
#include "aion/gameserver/services/toypet/PetMoodService.h"
#include "aion/gameserver/services/toypet/PetService.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets::testing::team {
namespace {

using model::gameobjects::Pet;
using model::gameobjects::PetSpecialFunction;
using model::gameobjects::player::PetCommonData;
using network::test::PacketWriter;
using serverpackets::SM_PET;
using services::toypet::PetAdoptionService;
using services::toypet::PetMoodService;
using services::toypet::PetService;

constexpr int32_t LOOTING_PET = 900001;
constexpr int32_t WAREHOUSE_PET = 900002;
constexpr int32_t MERCHANT_PET = 900010;
constexpr int32_t SM_PET_OPCODE = opcodeOf<SM_PET>;
/** AionClientPacketFactory packets[22] (ClientPacketInfo.gen.inc:37) */
constexpr int32_t CM_PET_OPCODE = 22;
/** packets[21] (ClientPacketInfo.gen.inc:36) */
constexpr int32_t CM_PET_EMOTE_OPCODE = 21;
/** PetAction ids (PetAction.java: ADOPT 1, DISMISS 4, FOOD 9, RENAME 10, MOOD 12) */
constexpr int32_t ACTION_ADOPT = 1;
constexpr int32_t ACTION_DISMISS = 4;
constexpr int32_t ACTION_FOOD = 9;
constexpr int32_t ACTION_RENAME = 10;
constexpr int32_t ACTION_MOOD = 12;

class PetServicesTest : public TeamTest {
protected:
	void SetUp() override {
		TeamTest::SetUp();
		::aion::gameserver::testing::pets::publishPetData();
		// NameConfig's defaults (NameConfig.java: gameserver.name.pet_pattern "[a-zA-Z]{2,16}", no forbidden sequence, no forbidden words): the
		// test process loads no properties
		configs::main::NameConfig::PET_NAME_PATTERN.set(std::wregex(L"[a-zA-Z]{2,16}"));
		configs::main::NameConfig::FORBIDDEN_SEQUENCE_PATTERN.set(std::nullopt);
		configs::main::NameConfig::FORBIDDEN_WORDS.set({});
	}

	void TearDown() override {
		for (Member& m : members)
			m.player().setPet(nullptr); // the pet holds its master: the cycle the controller's delete would cut
		pets.clear();
		TeamTest::TearDown();
		::aion::gameserver::testing::pets::resetPetData();
	}

	/** PetAdoptionService.addPet, then the Pet object VisibleObjectSpawner.spawnPet would bring into the world, given to its master */
	Pet& summon(Member& m, int32_t templateId) {
		PetAdoptionService::addPet(m.player(), templateId, "kitty", 0, 0);
		runtime::Ptr<PetCommonData> data = m.player().getPetList().getPet(templateId);
		EXPECT_TRUE(data);
		runtime::Ref<Pet> pet = model::gameobjects::VisibleObject::create<Pet>(dataholders::DataManager::PET_DATA->getPetTemplate(templateId),
			std::make_unique<controllers::PetController>(), *data, m.player());
		m.player().setPet(pet);
		pets.push_back(pet);
		return *pet;
	}

	void run(Member& m, const std::vector<uint8_t>& body) {
		Driver<CM_PET> packet(CM_PET_OPCODE);
		packet.readAndRun(body, m.client->get());
	}

	std::vector<runtime::Ref<Pet>> pets;
};

/** PetAdoptionService.java:57-64, :88-98: the name converted, the adopt and the surrender packets, the list */
TEST_F(PetServicesTest, APetIsAddedAndSurrendered) {
	Member& a = addMember("Alpha");
	PetAdoptionService::addPet(a.player(), LOOTING_PET, "kITTY", 0, 0);
	runtime::Ptr<PetCommonData> data = a.player().getPetList().getPet(LOOTING_PET);
	ASSERT_TRUE(data);
	EXPECT_EQ(data->getName(), "Kitty") << "Util.convertName";
	EXPECT_EQ(a.count(SM_PET(*data, true)), 1);
	EXPECT_TRUE(data->getFeedProgress()) << "a FOOD pet has a feed progress";

	PetAdoptionService::surrenderPet(a.player(), WAREHOUSE_PET);
	EXPECT_EQ(a.count(SM_PET_OPCODE), 1) << "no pet of that template: nothing";
	PetAdoptionService::surrenderPet(a.player(), LOOTING_PET);
	EXPECT_FALSE(a.player().getPetList().hasPet(LOOTING_PET));
	EXPECT_EQ(a.count(SM_PET(*data, false)), 1);
}

/** PetService.java:53-61: the name converted and broadcast to the master too */
TEST_F(PetServicesTest, ASummonedPetIsRenamed) {
	Member& a = addMember("Alpha");
	Pet& pet = summon(a, LOOTING_PET);
	a.clearSent();
	PetService::getInstance().renamePet(a.player(), "tIGER");
	EXPECT_EQ(pet.getCommonData()->getName(), "Tiger");
	EXPECT_EQ(a.count(SM_PET(pet.getObjectId(), "Tiger")), 1);
}

/** PetService.java:222-246: auto loot needs a LOOT function and no free-for-all team; auto sell a MERCHANT function */
TEST_F(PetServicesTest, AutoLootAndAutoSellNeedTheirFunctions) {
	Member& a = addMember("Alpha");
	Pet& looter = summon(a, LOOTING_PET);
	a.clearSent();
	PetService::getInstance().activateLoot(looter, true);
	EXPECT_EQ(a.count(SM_SYSTEM_MESSAGE::STR_MSG_LOOTING_PET_MESSAGE01()), 1);
	EXPECT_EQ(a.count(SM_PET(PetSpecialFunction::AUTOLOOT, true)), 1);
	EXPECT_TRUE(looter.getCommonData()->isLooting());
	PetService::getInstance().activateLoot(looter, false);
	EXPECT_FALSE(looter.getCommonData()->isLooting()) << "switching off needs no function";

	PetService::getInstance().activateAutoSell(looter, true);
	EXPECT_EQ(a.count(SM_PET(PetSpecialFunction::AUTOSELL, true)), 0) << "no MERCHANT function: audited, nothing sent";
	EXPECT_FALSE(looter.getCommonData()->isSelling());

	Member& b = addMember("Bravo");
	Pet& keeper = summon(b, WAREHOUSE_PET);
	b.clearSent();
	PetService::getInstance().activateLoot(keeper, true);
	EXPECT_TRUE(b.sent().empty()) << "no LOOT function: audited, nothing sent";
	EXPECT_FALSE(keeper.getCommonData()->isLooting());

	Member& c = addMember("Charlie");
	Pet& merchant = summon(c, MERCHANT_PET);
	c.clearSent();
	PetService::getInstance().activateAutoSell(merchant, true);
	EXPECT_TRUE(merchant.getCommonData()->isSelling());
	EXPECT_EQ(c.count(SM_PET(PetSpecialFunction::AUTOSELL, true)), 1);
}

/** PetMoodService.java:20-80: the check answers the mood, a shuggle counts once per 10 minutes, a present needs 9000 points */
TEST_F(PetServicesTest, TheMoodArms) {
	Member& a = addMember("Alpha");
	Pet& pet = summon(a, LOOTING_PET);
	a.clearSent();
	PetMoodService::checkMood(pet, 0, 0);
	EXPECT_EQ(a.count(SM_PET_OPCODE), 1) << "startCheckingMood: SM_PET(pet, 0, 0)";
	PetMoodService::checkMood(pet, 1, 37);
	EXPECT_EQ(pet.getCommonData()->getShuggleCounter(), 1);
	EXPECT_EQ(a.count(SM_PET_OPCODE), 3) << "interactWithPet: SM_PET(pet, 2, 37), SM_PET(pet, 4, 0)";
	PetMoodService::checkMood(pet, 1, 37);
	EXPECT_EQ(pet.getCommonData()->getShuggleCounter(), 1) << "the 10-minute cooldown";
	EXPECT_EQ(a.count(SM_PET_OPCODE), 3);
	PetMoodService::checkMood(pet, 3, 0);
	EXPECT_EQ(a.count(SM_PET_OPCODE), 3) << "requestPresent below 9000 points only logs";
	PetMoodService::checkMood(pet, 2, 0);
	EXPECT_EQ(a.count(SM_PET_OPCODE), 3) << "type 2 is no arm";
}

/** PetService.java:139-142: a pet without a doping bag ignores doping */
TEST_F(PetServicesTest, APetWithoutADopingBagIgnoresDoping) {
	Member& a = addMember("Alpha");
	Pet& pet = summon(a, LOOTING_PET);
	a.clearSent();
	PetService::getInstance().useDoping(pet, 0, 160010001, 0, 0);
	EXPECT_TRUE(a.sent().empty());
}

/** CM_PET.java:50-115 FOOD, :140-157: no pet does nothing; object 0 cancels the feeding; a pending refeed answers SM_PET(8, ...) */
TEST_F(PetServicesTest, ThePacketsFoodArms) {
	Member& a = addMember("Alpha");
	run(a, PacketWriter().H(ACTION_FOOD).D(1).D(0).D(0).D(0).data);
	EXPECT_TRUE(a.sent().empty()) << "no pet";
	Pet& pet = summon(a, LOOTING_PET);
	a.clearSent();
	run(a, PacketWriter().H(ACTION_FOOD).D(1).D(0).D(5).D(0).data);
	EXPECT_TRUE(pet.getCommonData()->getCancelFeed());
	EXPECT_EQ(a.count(SM_PET(4, 0, 0, pet)), 1);
	EXPECT_EQ(a.count(serverpackets::SM_EMOTION(a.player(), model::EmotionType::END_FEEDING, 0, a.player().getObjectId())), 1);

	pet.getCommonData()->setRefeedTime(commons::utils::currentTimeMillis() + 600'000);
	a.clearSent();
	run(a, PacketWriter().H(ACTION_FOOD).D(1).D(4711).D(3).D(0).data);
	EXPECT_EQ(a.count(SM_PET(8, 4711, 3, pet)), 1) << "not hungry yet";
}

/** CM_PET.java RENAME, MOOD, DISMISS and ADOPT: an invalid name is refused, the mood check runs, a dismiss without a pet does nothing */
TEST_F(PetServicesTest, ThePacketsNameMoodAndDismissArms) {
	Member& a = addMember("Alpha");
	run(a, PacketWriter().H(ACTION_RENAME).D(0).S("x").data);
	EXPECT_EQ(a.count(SM_SYSTEM_MESSAGE::STR_MSG_PET_NOT_AVALIABE_NAME()), 1) << "NameRestrictionService.isValidPetName";
	run(a, PacketWriter().H(ACTION_ADOPT).D(1).D(LOOTING_PET).C(0).D(0).D(0).D(0).D(0).S("x").data);
	EXPECT_EQ(a.count(SM_SYSTEM_MESSAGE::STR_MSG_PET_NOT_AVALIABE_NAME()), 2);
	run(a, PacketWriter().H(ACTION_DISMISS).D(LOOTING_PET).data);
	run(a, PacketWriter().H(ACTION_MOOD).D(0).D(0).data);
	EXPECT_EQ(a.count(SM_PET_OPCODE), 0) << "no pet";

	summon(a, LOOTING_PET);
	a.clearSent();
	run(a, PacketWriter().H(ACTION_MOOD).D(0).D(0).data);
	EXPECT_EQ(a.count(SM_PET_OPCODE), 1) << "subType 0 with no mood cooldown: startCheckingMood";
	run(a, PacketWriter().H(ACTION_RENAME).D(0).S("Rexy").data);
	EXPECT_EQ(a.player().getPet()->getCommonData()->getName(), "Rexy");
}

/**
 * CM_PET_EMOTE.java:36-58, :60-67: each emote's fields are read; without a pet, or with one not spawned, nothing happens (the moves of a
 * spawned pet need a map region: the gate's Z11)
 */
TEST_F(PetServicesTest, APetEmoteNeedsASpawnedPet) {
	Member& a = addMember("Alpha");
	const auto emote = [&](const std::vector<uint8_t>& body) {
		Driver<CM_PET_EMOTE> packet(CM_PET_EMOTE_OPCODE);
		packet.readAndRun(body, a.client->get());
	};
	emote(PacketWriter().C(0).F(1).F(2).F(3).C(4).data);                     // MOVE_STOP
	emote(PacketWriter().C(12).F(1).F(2).F(3).C(4).F(5).F(6).F(7).data);     // MOVETO
	emote(PacketWriter().C(255).C(1).C(2).data);                              // UNKNOWN
	summon(a, LOOTING_PET);
	a.clearSent();
	emote(PacketWriter().C(0).F(1).F(2).F(3).C(4).data);
	emote(PacketWriter().C(133).C(1).C(2).data); // EMOTION
	EXPECT_TRUE(a.sent().empty()) << "the pet is not spawned";
}

} // namespace
} // namespace aion::gameserver::network::aion::clientpackets::testing::team
