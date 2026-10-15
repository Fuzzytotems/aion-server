// P5-10e/P5-13, M5j stage 4 CP2 (m5j-plan.md §18.5, X-01): the matchmaker's services - AutoGroupService's registration, cancellation, the
// stop of a mask's registrations and the logout; PeriodicInstanceManager's opening, closing, icon request and login notice; PvPArenaService's
// tickets. The registrations use mask 2 (Chantra Dredgion: a periodic PvP instance) with AutoGroupTestSupport.h's level-1 test row and a
// member count of 2 a side, so a queue never fills (no instance is created: InstanceService needs the instance map, which the gate covers).
//
// Java: services/AutoGroupService.java, services/instance/PeriodicInstanceManager.java, services/instance/PvPArenaService.java.

#include "AutoGroupTestSupport.h"

#include <chrono>
#include <string>

#include "aion/gameserver/dataholders/ItemData.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/services/AutoGroupService.h"
#include "aion/gameserver/services/instance/PeriodicInstanceManager.h"
#include "aion/gameserver/services/instance/PvPArenaService.h"
#include "aion/gameserver/world/World.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets::testing::team {
namespace {

using services::AutoGroupService;
using services::instance::PeriodicInstanceManager;
using services::instance::PvPArenaService;

constexpr int32_t CHANTRA = 2;           // AutoGroupType.CHANTRA_DREDGION's mask (AutoGroupType.java:19)
constexpr int32_t CHANTRA_WORLD = 390000002; // its test row's world

/** item_templates.xml shape: the three arena tickets the arenas take (AutoPvPFFAInstance.java:44-48, AutoHarmonyInstance.java:50) */
constexpr std::string_view TICKET_ROWS = R"xml(
	<item_template id="186000135" name="Arena Ticket" level="1" cName="ticket_arena" mask="12414" max_stack_count="1000" quality="COMMON" price="1" desc="1"/>
	<item_template id="186000184" name="Harmony Ticket" level="1" cName="ticket_harmony" mask="12414" max_stack_count="1000" quality="COMMON" price="1" desc="1"/>
	<item_template id="186000185" name="Glory Ticket" level="1" cName="ticket_glory" mask="12414" max_stack_count="1000" quality="COMMON" price="1" desc="1"/>
</item_templates>)xml";

class AutoGroupServiceTest : public AutoGroupTest {
protected:
	void SetUp() override {
		AutoGroupTest::SetUp();
		std::string itemsXml(items::ITEM_TEMPLATES_XML);
		itemsXml.replace(itemsXml.rfind("</item_templates>"), std::string_view("</item_templates>").size(), TICKET_ROWS);
		dataholders::DataManager::ITEM_DATA.resetForTests();
		dataholders::DataManager::ITEM_DATA.publish(xml::bindString<dataholders::ItemData>(itemContext, itemsXml));
	}

	void TearDown() override {
		for (Member& m : members)
			world::World::getInstance().removeObject(m.player());
		AutoGroupTest::TearDown();
	}

	/** a member stored in the World (World.getPlayer finds him: the matchmaker's windows and broadcasts) */
	Member& online(std::string_view name, float x = 100.0f) {
		Member& m = addMember(name, x);
		world::World::getInstance().storeObject(m.player());
		return m;
	}

	void give(Member& m, int32_t objId, int32_t itemId, int64_t count) {
		runtime::Ref<model::gameobjects::Item> item = items::loadedItem(objId, itemId, count, model::items::storage::StorageType::CUBE, objId % 100);
		m.player().getStorage(model::items::storage::getId(model::items::storage::StorageType::CUBE))->onLoadHandler(*item);
		storedItems.push_back(item);
	}

	xml::LoadContext itemContext;
};

// ---- AutoGroupService ------------------------------------------------------------------------------------------------------------------------

/** AutoGroupService.java:51-54, :287-305: an unknown mask, the level, the cooldown and the arena ticket refuse the registration */
TEST_F(AutoGroupServiceTest, RegistrationRefusals) {
	Member& a = online("Alpha");
	AutoGroupService& service = AutoGroupService::getInstance();
	service.startLooking(a.player(), 999, EntryRequestType::NEW_GROUP_ENTRY);
	EXPECT_TRUE(a.sent().empty()) << "no AutoGroupType of the mask";
	service.startLooking(a.player(), 21, EntryRequestType::NEW_GROUP_ENTRY);
	EXPECT_EQ(a.count(SM_SYSTEM_MESSAGE::STR_MSG_CANT_INSTANCE_ENTER_LEVEL()), 1) << "mask 21: levels 46-50";
	service.startLooking(a.player(), 22, EntryRequestType::NEW_GROUP_ENTRY);
	EXPECT_EQ(a.count(SM_SYSTEM_MESSAGE::STR_MSG_INSTANCE_CANT_ENTER_WITHOUT_ITEM()), 1) << "the Arena of Chaos 2 without a ticket";
	cooldown(a, CHANTRA_WORLD);
	service.startLooking(a.player(), CHANTRA, EntryRequestType::NEW_GROUP_ENTRY);
	EXPECT_EQ(a.count(SM_SYSTEM_MESSAGE::STR_MSG_CANNOT_MAKE_INSTANCE_COOL_TIME()), 1);
	EXPECT_FALSE(service.isSearching(a.player(), CHANTRA));
	EXPECT_EQ(a.count(opcodeOf<serverpackets::SM_AUTO_GROUP>), 0);
}

/** AutoGroupService.java:51-79, :326-345: a registration, its double, the leader's cancellation and the penalty's end */
TEST_F(AutoGroupServiceTest, SoloRegistrationAndCancellation) {
	Member& a = online("Alpha");
	AutoGroupService& service = AutoGroupService::getInstance();
	service.startLooking(a.player(), CHANTRA, EntryRequestType::NEW_GROUP_ENTRY);
	EXPECT_EQ(a.count(SM_AUTO_GROUP(CHANTRA, 6, true)), 1) << "a periodic instance";
	EXPECT_EQ(a.count(SM_SYSTEM_MESSAGE::STR_MSG_INSTANCE_REGISTER_SUCCESS()), 1);
	EXPECT_EQ(a.count(SM_AUTO_GROUP(CHANTRA, 1, 0, "Alpha")), 1);
	EXPECT_TRUE(service.isSearching(a.player(), CHANTRA)) << "one Elyos of 2 + 2: queued, no instance";
	service.startLooking(a.player(), CHANTRA, EntryRequestType::NEW_GROUP_ENTRY);
	EXPECT_EQ(a.count(SM_SYSTEM_MESSAGE::STR_MSG_CANT_INSTANCE_ALREADY_REGISTERED(CHANTRA_WORLD)), 1);
	a.clearSent();
	service.cancelRegistration(a.player(), CHANTRA);
	EXPECT_EQ(a.count(SM_AUTO_GROUP(CHANTRA, 2)), 1) << "the leader's cancel: window 2 to the party";
	EXPECT_FALSE(service.isSearching(a.player(), CHANTRA));
	service.cancelRegistration(a.player(), CHANTRA); // no entry: nothing
	EXPECT_EQ(a.count(SM_AUTO_GROUP(CHANTRA, 2)), 1);
	const size_t pending = executor->pendingTaskCount();
	EXPECT_GE(pending, 1u) << "the penalty's removal task";
	executor->advance(std::chrono::milliseconds(10000));
	EXPECT_LT(executor->pendingTaskCount(), pending) << "10 s later the penalty ends";
}

/** AutoGroupService.java:51-79, :330-345: a party registers through its leader; a member leaves alone, then the leader cancels for the rest */
TEST_F(AutoGroupServiceTest, PartyRegistration) {
	Member& a = online("Alpha");
	Member& b = online("Bravo", 101.0f);
	form({&a, &b});
	AutoGroupService& service = AutoGroupService::getInstance();
	service.startLooking(a.player(), CHANTRA, EntryRequestType::GROUP_ENTRY);
	for (Member* m : {&a, &b})
		EXPECT_EQ(m->count(SM_AUTO_GROUP(CHANTRA, 1, 2, "Alpha")), 1) << "GROUP_ENTRY's id 2, the leader's name";
	EXPECT_TRUE(service.isSearching(b.player(), CHANTRA));
	clearAll();
	service.cancelRegistration(b.player(), CHANTRA);
	EXPECT_EQ(b.count(SM_AUTO_GROUP(CHANTRA, 2)), 1);
	EXPECT_EQ(a.count(SM_AUTO_GROUP(CHANTRA, 2)), 0) << "a member's cancel: his own window only";
	EXPECT_FALSE(service.isSearching(b.player(), CHANTRA));
	EXPECT_TRUE(service.isSearching(a.player(), CHANTRA));
	service.cancelRegistration(a.player(), CHANTRA);
	EXPECT_EQ(a.count(SM_AUTO_GROUP(CHANTRA, 2)), 1);
	EXPECT_FALSE(service.isSearching(a.player(), CHANTRA));
}

/** AutoGroupService.java:186-205, :320-324: a leader who logs out hands the party on; stopping the mask closes every window */
TEST_F(AutoGroupServiceTest, LogoutAndStop) {
	Member& a = online("Alpha");
	Member& b = online("Bravo", 101.0f);
	Member& c = online("Charlie", 102.0f);
	form({&a, &b});
	AutoGroupService& service = AutoGroupService::getInstance();
	service.startLooking(a.player(), CHANTRA, EntryRequestType::GROUP_ENTRY);
	service.startLooking(c.player(), CHANTRA, EntryRequestType::QUICK_GROUP_ENTRY);
	EXPECT_TRUE(service.isSearching(c.player(), CHANTRA));
	service.onLogout(a.player());
	EXPECT_TRUE(service.isSearching(a.player(), CHANTRA)) << "still a member of the entry: only the leader changed";
	clearAll();
	service.onLogout(c.player());
	EXPECT_FALSE(service.isSearching(c.player(), CHANTRA)) << "alone: the entry is removed";
	EXPECT_EQ(c.count(opcodeOf<serverpackets::SM_AUTO_GROUP>), 0);
	service.stopRegistrationsByMaskId(CHANTRA);
	EXPECT_EQ(a.count(SM_AUTO_GROUP(CHANTRA, 2)), 1);
	EXPECT_EQ(b.count(SM_AUTO_GROUP(CHANTRA, 2)), 1);
	EXPECT_FALSE(service.isSearching(b.player(), CHANTRA));
}

// ---- PeriodicInstanceManager -------------------------------------------------------------------------------------------------------------------

/** PeriodicInstanceManager.java:64-139: the opening's icon and message to the players of the level range, the icon request, the login notice,
 * the close after the registration period */
TEST_F(AutoGroupServiceTest, PeriodicRegistration) {
	Member& a = online("Alpha");
	Member& b = online("Bravo", 101.0f);
	PeriodicInstanceManager& manager = PeriodicInstanceManager::getInstance();
	SM_SYSTEM_MESSAGE opening = SM_SYSTEM_MESSAGE::STR_MSG_INSTANCE_OPEN_IDDREADGION_02();
	manager.handleRequest(a.player(), CHANTRA);
	EXPECT_EQ(a.count(SM_AUTO_GROUP(CHANTRA)), 0) << "closed: no answer";
	EXPECT_TRUE(manager.openRegistration(opening, CHANTRA, 1));
	EXPECT_TRUE(manager.isRegistrationOpen(CHANTRA));
	for (Member* m : {&a, &b}) {
		EXPECT_EQ(m->count(SM_AUTO_GROUP(CHANTRA, 6, false)), 1);
		EXPECT_EQ(m->count(SM_SYSTEM_MESSAGE::STR_MSG_INSTANCE_OPEN_IDDREADGION_02()), 1);
	}
	EXPECT_FALSE(manager.openRegistration(opening, CHANTRA, 1)) << "already open";
	manager.handleRequest(a.player(), CHANTRA);
	EXPECT_EQ(a.count(SM_AUTO_GROUP(CHANTRA)), 1);
	clearAll();
	manager.checkAndSendOpenRegistrations(a.player().getObjectId());
	EXPECT_EQ(a.count(SM_AUTO_GROUP(CHANTRA, 6, false)), 1) << "the login notice";
	cooldown(b, CHANTRA_WORLD);
	manager.checkAndSendOpenRegistrations(b.player());
	EXPECT_EQ(b.count(SM_AUTO_GROUP(CHANTRA, 6, false)), 0) << "on cooldown: no icon";
	AutoGroupService::getInstance().startLooking(a.player(), CHANTRA, EntryRequestType::NEW_GROUP_ENTRY);
	clearAll();
	executor->advance(std::chrono::milliseconds(59999));
	EXPECT_TRUE(manager.isRegistrationOpen(CHANTRA));
	executor->advance(std::chrono::milliseconds(1));
	EXPECT_FALSE(manager.isRegistrationOpen(CHANTRA)) << "1 minute";
	EXPECT_EQ(a.count(SM_AUTO_GROUP(CHANTRA, 6, true)), 1) << "the closed icon";
	EXPECT_EQ(a.count(SM_AUTO_GROUP(CHANTRA, 2)), 1) << "stopRegistrationsByMaskId closed A's registration";
	EXPECT_FALSE(AutoGroupService::getInstance().isSearching(a.player(), CHANTRA));
	EXPECT_FALSE(manager.closeRegistration(CHANTRA)) << "already closed";
}

// ---- PvPArenaService -----------------------------------------------------------------------------------------------------------------------------

/** PvPArenaService.java:19-43: one ticket for the chaos and discipline arenas and harmony's own, three for glory; other types need none */
TEST_F(AutoGroupServiceTest, ArenaTickets) {
	Member& a = online("Alpha");
	EXPECT_TRUE(PvPArenaService::checkItem(a.player(), AutoGroupType::KAMAR_BATTLEFIELD));
	EXPECT_FALSE(PvPArenaService::checkItem(a.player(), AutoGroupType::ARENA_OF_DISCIPLINE_2));
	EXPECT_FALSE(PvPArenaService::isPvPArenaAvailable(a.player(), AutoGroupType::ARENA_OF_CHAOS_2));
	EXPECT_EQ(a.count(SM_SYSTEM_MESSAGE::STR_MSG_INSTANCE_CANT_ENTER_WITHOUT_ITEM()), 1);
	give(a, 9401, 186000135, 1);
	EXPECT_TRUE(PvPArenaService::checkItem(a.player(), AutoGroupType::ARENA_OF_DISCIPLINE_2));
	EXPECT_TRUE(PvPArenaService::isPvPArenaAvailable(a.player(), AutoGroupType::ARENA_OF_CHAOS_2)) << "START_TIME_ENABLE is off";
	EXPECT_FALSE(PvPArenaService::checkItem(a.player(), AutoGroupType::ARENA_OF_HARMONY_2)) << "harmony's own ticket";
	give(a, 9402, 186000184, 1);
	EXPECT_TRUE(PvPArenaService::checkItem(a.player(), AutoGroupType::ARENA_OF_HARMONY_2));
	give(a, 9403, 186000185, 2);
	EXPECT_FALSE(PvPArenaService::checkItem(a.player(), AutoGroupType::ARENA_OF_GLORY_1)) << "2 of 3";
	give(a, 9404, 186000185, 1);
	EXPECT_TRUE(PvPArenaService::checkItem(a.player(), AutoGroupType::ARENA_OF_GLORY_1));
}

} // namespace
} // namespace aion::gameserver::network::aion::clientpackets::testing::team
