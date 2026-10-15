// The stage-2 item and pet commands (m5j-plan.md §5.4, §18.3 stage 2, item E-07): //pet (CP1). Real Players with real AionConnections
// (CommandTestSupport.h); the pet templates are the test pets of tests/playersvc/PetTestSupport.h (by relative path). This executable has no
// database: PlayerPetsDAO logs and carries on, the cases read the in-memory pet list. The texts are the Java literals of Pet.java.

#include "CommandTestSupport.h"

#include "../../playersvc/PetTestSupport.h"

#include <algorithm>
#include <map>
#include <string>
#include <vector>

#include "aion/gameserver/configs/administration/CommandsConfig.h"
#include "aion/gameserver/handlers/admincommands/Pet.h"
#include "aion/gameserver/model/gameobjects/player/PetCommonData.h"
#include "aion/gameserver/model/gameobjects/player/PetList.h"
#include "aion/gameserver/runtime/base/Exceptions.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets::testing {
namespace {

class ItemPetCommandsTest : public CommandTest {
protected:
	void SetUp() override {
		CommandTest::SetUp();
		std::map<std::string, int8_t, std::less<>> levels(*configs::administration::CommandsConfig::ACCESS_LEVELS.get());
		levels["pet"] = 3;
		configs::administration::CommandsConfig::ACCESS_LEVELS.set(levels);
		::aion::gameserver::testing::pets::publishPetData();
	}

	void TearDown() override {
		CommandTest::TearDown();
		::aion::gameserver::testing::pets::resetPetData();
	}
};

/** Pet.java:28-66: the list, add with and without a name, an unknown id, del */
TEST_F(ItemPetCommandsTest, PetListsAddsAndDeletes) {
	Player& gm = connected(732000, "Warden", 3);
	handlers::admincommands::Pet pet;

	EXPECT_TRUE(pet.process(gm, args({"add", "123"})));
	EXPECT_EQ(client()->sentBytes(), exactly({message("Invalid pet ID.")}));
	client()->clearSent();
	EXPECT_TRUE(pet.process(gm, args({"add", "900001"})));
	EXPECT_EQ(client()->sentBytes(), exactly({message("You must specify a name for the pet.")}));
	EXPECT_FALSE(gm.getPetList().hasPet(900001));
	EXPECT_TRUE(pet.process(gm, args({"add", "900001", "kitty"})));
	EXPECT_TRUE(gm.getPetList().hasPet(900001)) << "PetAdoptionService.addPet";
	EXPECT_EQ(gm.getPetList().getPet(900001)->getName(), "Kitty");
	EXPECT_TRUE(pet.process(gm, args({"del", "900001"})));
	EXPECT_FALSE(gm.getPetList().hasPet(900001)) << "PetAdoptionService.surrenderPet";
	EXPECT_TRUE(pet.process(gm, args({"other", "900002"})));
	EXPECT_FALSE(gm.getPetList().hasPet(900002)) << "an unknown action with a valid id does nothing";
	EXPECT_THROW(pet.execute(gm, std::vector<std::string>{"add"}), runtime::ArrayIndexOutOfBoundsException) << "params[1]";
}

/** whether the packet's bytes hold the text as UTF-16LE (an SM_MESSAGE body writes its text with writeS) */
bool holdsText(const std::vector<uint8_t>& bytes, std::string_view text) {
	std::vector<uint8_t> needle;
	for (char c : text) {
		needle.push_back(static_cast<uint8_t>(c));
		needle.push_back(0);
	}
	return std::search(bytes.begin(), bytes.end(), needle.begin(), needle.end()) != bytes.end();
}

/** Pet.java:37-47: one text, ids sorted, each id with its functions */
TEST_F(ItemPetCommandsTest, PetListNamesEveryTemplate) {
	Player& gm = connected(732001, "Warden", 3);
	handlers::admincommands::Pet pet;
	EXPECT_TRUE(pet.process(gm, args({"list"})));
	const std::vector<std::vector<uint8_t>> sent = client()->sentBytes();
	ASSERT_EQ(sent.size(), 1u);
	EXPECT_TRUE(holdsText(sent[0], "List of pets:\n900001 - [color:"));
	EXPECT_TRUE(holdsText(sent[0], "\n\tFunctions: LOOT, FOOD"));
	EXPECT_TRUE(holdsText(sent[0], "\n900002 - [color:"));
	EXPECT_TRUE(holdsText(sent[0], "\n\tFunctions: FOOD, WAREHOUSE"));
	EXPECT_TRUE(holdsText(sent[0], "\n900010 - [color:"));
	EXPECT_TRUE(holdsText(sent[0], "\n\tFunctions: MERCHANT"));
}

} // namespace
} // namespace aion::gameserver::network::aion::clientpackets::testing
