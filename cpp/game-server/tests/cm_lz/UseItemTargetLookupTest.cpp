// M5c K-01/K-02 lane (m5c-plan.md §18.7, P5-16): CM_USE_ITEM's target-item lookup - the cube first, then the equipment
// (CM_USE_ITEM.java:67-72) - observed through the one action of the start maps that reads its target on the packet's own thread:
// ExtractAction.canAct (Extraction Tools, item_templates.xml:836407, verbatim in EconomyPacketTestSupport.h; ported in M5c stage 0, E-02).
// canAct answers a missing target with STR_DECOMPOSE_ITEM_NO_TARGET_ITEM, a target that is neither armour nor weapon with
// STR_DECOMPOSE_ITEM_IT_CAN_NOT_BE_DECOMPOSED(name), an equipped one with STR_DECOMPOSE_EQUIP_ITEM_CAN_NOT_BE_DECOMPOSED, and passes an
// unequipped weapon, whose act starts the 5 s extraction (SM_ITEM_USAGE_ANIMATION(tools, 5000, 0), ExtractAction.java:44). So which item the
// lookup found shows in what the player is sent:
// - a weapon in the cube passes;
// - a weapon only equipped is found in the equipment (the equipped refusal);
// - an object id held by the cube (a potion) and by the equipment (a sword) resolves to the cube's item.
// The house-object lookup of an id found in neither stays uncovered (UseItemPacketTest.cpp says why: it asks the database).

#include "../cm_ak/EconomyPacketTestSupport.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "aion/gameserver/controllers/PlayerController.h"
#include "aion/gameserver/network/aion/clientpackets/CM_USE_ITEM.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets::testing::items {
namespace {

using serverpackets::SM_SYSTEM_MESSAGE;

/** the decoded opcode of ClientPacketInfo.gen.inc:48 (Java AionClientPacketFactory.java:65, State.IN_GAME) */
constexpr int32_t CM_USE_ITEM_OPCODE = 37;
constexpr int64_t MAIN_HAND = 1; // ItemSlot.MAIN_HAND

constexpr int32_t TOOLS = 900001;  // 5 x Extraction Tools
constexpr int32_t TARGET = 900002; // the target's object id

class UseItemTargetLookupTest : public EconomyPacketTest {
protected:
	void SetUp() override {
		EconomyPacketTest::SetUp();
		stored(TOOLS, EXTRACTION_TOOLS, 5);
	}

	/** C_USE_ITEM with type 2: D item, C 2, D target (CM_USE_ITEM.java:38-52) */
	void useOn(std::optional<int32_t> target) {
		PacketWriter body;
		body.D(TOOLS);
		if (target)
			body.C(2).D(*target);
		else
			body.C(0);
		readAndRun<CM_USE_ITEM>(CM_USE_ITEM_OPCODE, body.data);
	}

	std::vector<uint8_t> message(SM_SYSTEM_MESSAGE&& packet) { return serializedFor(std::move(packet)); }

	/** What ExtractAction.canAct sent, or its act's start (the packets of the other opcodes are not the lookup's business) */
	std::vector<std::vector<uint8_t>> answers() {
		std::vector<std::vector<uint8_t>> result = packetsOf(sent(), SM_SYSTEM_MESSAGE_OPCODE);
		for (const std::vector<uint8_t>& animation : packetsOf(sent(), SM_ITEM_USAGE_ANIMATION_OPCODE))
			result.push_back(animation);
		return result;
	}

	void TearDown() override {
		if (f.player)
			f.player->getController().cancelUseItem(); // the extraction's 5 s task and its observer
		EconomyPacketTest::TearDown();
	}
};

TEST_F(UseItemTargetLookupTest, AWeaponInTheCubeIsTheTarget) {
	stored(TARGET, TRAINING_SWORD, 1);

	useOn(TARGET);

	EXPECT_EQ(answers(), exactly({usageAnimation(player().getObjectId(), TOOLS, EXTRACTION_TOOLS, 5000, 0, 0)}));
}

TEST_F(UseItemTargetLookupTest, AWeaponOnlyEquippedIsFoundInTheEquipment) {
	equipped(TARGET, TRAINING_SWORD, MAIN_HAND);

	useOn(TARGET);

	EXPECT_EQ(answers(), exactly({message(SM_SYSTEM_MESSAGE::STR_DECOMPOSE_EQUIP_ITEM_CAN_NOT_BE_DECOMPOSED())}));
}

TEST_F(UseItemTargetLookupTest, TheCubeIsAskedBeforeTheEquipment) {
	Item& potion = stored(TARGET, MINOR_LIFE_POTION, 1);
	equipped(TARGET, TRAINING_SWORD, MAIN_HAND); // the same object id in the equipment

	useOn(TARGET);

	EXPECT_EQ(answers(), exactly({message(SM_SYSTEM_MESSAGE::STR_DECOMPOSE_ITEM_IT_CAN_NOT_BE_DECOMPOSED(potion.getL10n()))}));
}

// CM_USE_ITEM.java:67: a target id of 0 (any type but 2) looks nothing up
TEST_F(UseItemTargetLookupTest, WithoutATargetIdNothingIsLookedUp) {
	stored(TARGET, TRAINING_SWORD, 1);

	useOn(std::nullopt);

	EXPECT_EQ(answers(), exactly({message(SM_SYSTEM_MESSAGE::STR_DECOMPOSE_ITEM_NO_TARGET_ITEM())}));
}

} // namespace
} // namespace aion::gameserver::network::aion::clientpackets::testing::items
