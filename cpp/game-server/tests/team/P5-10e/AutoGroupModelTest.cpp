// P5-10e, M5j stage 4 CP1 (m5j-plan.md §18.5, X-01): the matchmaking model - AutoGroupType's companion (its constructor data, lookups and
// predicates, the constant-specific newAutoInstance), EntryRequestType's, AGPlayer, LookingForParty, AutoInstance and its three subclasses'
// registration (addLookingForParty) and AutoGroupUtility's checks and packets. Real Players of TeamTestSupport.h in a Poeta map instance; the
// AUTO_GROUP rows are auto_group.xml's (the five used), the INSTANCE_COOLTIME_DATA rows are test values for their worlds.
//
// Java: model/autogroup/{AutoGroupType,EntryRequestType,AGPlayer,LookingForParty,AutoInstance,AutoPvpInstance,AutoPvPFFAInstance,
// AutoHarmonyInstance}.java, services/autogroup/AutoGroupUtility.java.

#include "AutoGroupTestSupport.h"

#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <thread>
#include <vector>

#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/dataholders/AutoGroupData.bind.h"
#include "aion/gameserver/dataholders/AutoGroupData.h"
#include "aion/gameserver/dataholders/InstanceCooltimeData.bind.h"
#include "aion/gameserver/dataholders/InstanceCooltimeData.h"
#include "aion/gameserver/model/Race.h"
#include "aion/gameserver/model/autogroup/AGPlayer.h"
#include "aion/gameserver/model/autogroup/AGQuestion.h"
#include "aion/gameserver/model/autogroup/AutoGroup.h"
#include "aion/gameserver/model/autogroup/AutoGroupTypeInfo.h"
#include "aion/gameserver/model/autogroup/AutoHarmonyInstance.h"
#include "aion/gameserver/model/autogroup/AutoPvPFFAInstance.h"
#include "aion/gameserver/model/autogroup/AutoPvpInstance.h"
#include "aion/gameserver/model/autogroup/EntryRequestTypeInfo.h"
#include "aion/gameserver/model/autogroup/LookingForParty.h"
#include "aion/gameserver/model/gameobjects/player/PortalCooldown.h"
#include "aion/gameserver/model/gameobjects/player/PortalCooldownList.h"
#include "aion/gameserver/world/WorldMap2DInstance.h"
#include "aion/gameserver/network/aion/serverpackets/SM_AUTO_GROUP.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/services/autogroup/AutoGroupUtility.h"
#include "aion/gameserver/world/World.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets::testing::team {
namespace {

/** AutoPvpInstance with the protected item removal reachable */
struct PvpProbe final : ag::AutoPvpInstance {
	AION_MAKE_REF_FRIEND
	using AutoPvpInstance::removeItem;
	static runtime::Ref<PvpProbe> make(AutoGroupType agt) { return runtime::makeRef<PvpProbe>(agt); }

protected:
	explicit PvpProbe(AutoGroupType agt) : AutoPvpInstance(agt) {}
	~PvpProbe() override = default;
};

class AutoGroupModelTest : public AutoGroupTest {};

// ---- AutoGroupType's companion -------------------------------------------------------------------------------------------------------------

/** AutoGroupType.java:249-384: the constructor data, the template of the mask id, the lookups in ordinal order and the predicates */
TEST_F(AutoGroupModelTest, AutoGroupTypeCompanion) {
	EXPECT_EQ(ag::getInstanceMaskId(AutoGroupType::KAMAR_BATTLEFIELD), 107);
	EXPECT_EQ(ag::getMaximumJoinTime(AutoGroupType::KAMAR_BATTLEFIELD), 230000);
	EXPECT_EQ(ag::getMaximumJoinTime(AutoGroupType::IDGEL_DOME), 170000);
	EXPECT_EQ(ag::getMaximumJoinTime(AutoGroupType::BARANATH_DREDGION), 420000);
	EXPECT_EQ(ag::getDifficultId(AutoGroupType::ARENA_OF_CHAOS_3), 3);
	EXPECT_EQ(ag::getDifficultId(AutoGroupType::HARAMONIOUS_TRAINING_CENTER_2), 2);
	EXPECT_EQ(ag::getDifficultId(AutoGroupType::BARANATH_DREDGION), 0) << "the two-argument constructor leaves it 0";
	ASSERT_NE(ag::getTemplate(AutoGroupType::KAMAR_BATTLEFIELD), nullptr);
	EXPECT_EQ(ag::getTemplate(AutoGroupType::KAMAR_BATTLEFIELD)->getInstanceMapId(), 301120000);
	EXPECT_EQ(ag::getL10nId(AutoGroupType::ARENA_OF_CHAOS_1), 402045);

	EXPECT_TRUE(ag::containNpcId(AutoGroupType::BARANATH_DREDGION, 279041));
	EXPECT_FALSE(ag::containNpcId(AutoGroupType::BARANATH_DREDGION, 730482));
	EXPECT_EQ(ag::getAGTByMaskId(21), std::optional(AutoGroupType::ARENA_OF_CHAOS_1));
	EXPECT_EQ(ag::getAutoGroup(730577), std::optional(AutoGroupType::ARENA_OF_HARMONY_1));
	EXPECT_EQ(ag::getAutoGroup(48, 730483), std::optional(AutoGroupType::ARENA_OF_DISCIPLINE_1));
	EXPECT_EQ(ag::getAutoGroup(51, 730483), std::nullopt) << "level 51: above 50";
	EXPECT_EQ(ag::getAutoGroupByWorld(61, 301120000), std::optional(AutoGroupType::KAMAR_BATTLEFIELD));
	EXPECT_EQ(ag::getAutoGroupByWorld(60, 301120000), std::nullopt);
	EXPECT_TRUE(ag::isInLvlRange(AutoGroupType::KAMAR_BATTLEFIELD, 65));
	EXPECT_FALSE(ag::isInLvlRange(AutoGroupType::KAMAR_BATTLEFIELD, 66));

	EXPECT_TRUE(ag::isPeriodicInstance(AutoGroupType::IDGEL_DOME));
	EXPECT_TRUE(ag::isPeriodicInstance(AutoGroupType::TERATH_DREDGION));
	EXPECT_FALSE(ag::isPeriodicInstance(AutoGroupType::ARENA_OF_GLORY_1));
	EXPECT_TRUE(ag::isPvPSoloArena(AutoGroupType::ARENA_OF_DISCIPLINE_4));
	EXPECT_TRUE(ag::isTrainingPvPSoloArena(AutoGroupType::DISCIPLINE_TRAINING_GROUNDS_2));
	EXPECT_TRUE(ag::isPvPFFAArena(AutoGroupType::ARENA_OF_CHAOS_4));
	EXPECT_TRUE(ag::isTrainingPvPFFAArena(AutoGroupType::CHAOS_TRAINING_GROUNDS_1));
	EXPECT_TRUE(ag::isTrainingHarmonyArena(AutoGroupType::HARAMONIOUS_TRAINING_CENTER_4));
	EXPECT_TRUE(ag::isHarmonyArena(AutoGroupType::ARENA_OF_HARMONY_3));
	EXPECT_TRUE(ag::isGloryArena(AutoGroupType::ARENA_OF_GLORY_2));
	EXPECT_TRUE(ag::isPvpArena(AutoGroupType::DISCIPLINE_TRAINING_GROUNDS_4));
	EXPECT_FALSE(ag::isPvpArena(AutoGroupType::ARENA_OF_HARMONY_1)) << "harmony and glory are not PvP arenas";
	EXPECT_FALSE(ag::isPvpArena(AutoGroupType::ARENA_OF_GLORY_1));

	EXPECT_NE(dynamic_cast<ag::AutoPvpInstance*>(ag::createAutoInstance(AutoGroupType::KAMAR_BATTLEFIELD).get()), nullptr);
	EXPECT_NE(dynamic_cast<ag::AutoPvpInstance*>(ag::createAutoInstance(AutoGroupType::CHANTRA_DREDGION).get()), nullptr);
	EXPECT_NE(dynamic_cast<ag::AutoPvPFFAInstance*>(ag::createAutoInstance(AutoGroupType::ARENA_OF_GLORY_2).get()), nullptr);
	EXPECT_NE(dynamic_cast<ag::AutoHarmonyInstance*>(ag::createAutoInstance(AutoGroupType::HARAMONIOUS_TRAINING_CENTER_1).get()), nullptr);
	EXPECT_EQ(ag::createAutoInstance(AutoGroupType::ARENA_OF_CHAOS_2)->getAutoGroupType(), AutoGroupType::ARENA_OF_CHAOS_2);

	EXPECT_EQ(ag::getId(EntryRequestType::QUICK_GROUP_ENTRY), 1);
	EXPECT_EQ(ag::getTypeById(2), std::optional(EntryRequestType::GROUP_ENTRY));
	EXPECT_EQ(ag::getTypeById(3), std::nullopt);

	// without a template of its mask (Java: the constructor's warning, then a NullPointerException on use); a walk reaching it throws too
	dataholders::DataManager::AUTO_GROUP.resetForTests();
	xml::LoadContext withoutContext;
	dataholders::DataManager::AUTO_GROUP.publish(xml::bindString<dataholders::AutoGroupData>(withoutContext, autoGroupXml(false)));
	EXPECT_EQ(ag::getTemplate(AutoGroupType::IDGEL_DOME), nullptr);
	EXPECT_THROW(ag::getL10nId(AutoGroupType::IDGEL_DOME), runtime::NullPointerException);
	EXPECT_THROW(ag::containNpcId(AutoGroupType::IDGEL_DOME, 1), runtime::NullPointerException);
	EXPECT_EQ(ag::getAutoGroup(730577), std::optional(AutoGroupType::ARENA_OF_HARMONY_1)) << "found before IDGEL_DOME, the last constant";
	EXPECT_THROW(ag::getAutoGroup(1), runtime::NullPointerException) << "a walk to the end reaches IDGEL_DOME";
	EXPECT_THROW(ag::createAutoInstance(AutoGroupType::IDGEL_DOME)->getMaxPlayers(), runtime::NullPointerException);
}

// ---- AGPlayer and LookingForParty ----------------------------------------------------------------------------------------------------------

/** AGPlayer.java:12-14, LookingForParty.java:24-96: a solo player or the online members of his team; leader, members, the start-enter window */
TEST_F(AutoGroupModelTest, LookingForPartyMembers) {
	Member& a = addMember("Alpha");
	Member& b = addMember("Bravo", 101.0f);
	Member& c = addMember("Charlie", 102.0f);
	runtime::Ref<LookingForParty> alone = solo(a, EntryRequestType::QUICK_GROUP_ENTRY, 21);
	ASSERT_EQ(alone->getMembers().size(), 1);
	runtime::Ptr<ag::AGPlayer> agp = alone->getMembers().get(a.player().getObjectId());
	ASSERT_NE(agp, nullptr);
	EXPECT_EQ(agp->name(), "Alpha");
	EXPECT_EQ(agp->race(), Race::ELYOS);
	EXPECT_EQ(agp->playerClass(), a.player().getPlayerClass());
	EXPECT_EQ(alone->getRace(), Race::ELYOS);
	EXPECT_EQ(alone->getMaskId(), 21);
	EXPECT_EQ(alone->getEntryRequestType(), EntryRequestType::QUICK_GROUP_ENTRY);
	EXPECT_TRUE(alone->isLeader(a.player().getObjectId()));
	EXPECT_FALSE(alone->isOnStartEnterTask()) << "no start time: long ago";
	alone->setStartEnterTime();
	EXPECT_TRUE(alone->isOnStartEnterTask()) << "within 120 s";

	form({&a, &b, &c});
	runtime::Ref<LookingForParty> party = solo(a, EntryRequestType::GROUP_ENTRY);
	EXPECT_EQ(party->getMembers().size(), 3) << "the online team members";
	EXPECT_TRUE(party->isMember(c.player().getObjectId()));
	party->unregisterMember(c.player().getObjectId());
	EXPECT_FALSE(party->isMember(c.player().getObjectId()));
	party->unregisterMember(std::nullopt);
	EXPECT_EQ(party->getMembers().size(), 2);
	party->setLeaderObjId(b.player().getObjectId());
	EXPECT_TRUE(party->isLeader(b.player().getObjectId()));
	EXPECT_FALSE(party->isLeader(a.player().getObjectId()));
}

/** LookingForParty.java:88-96: the higher entry type first, then the larger party, then the earlier registration */
TEST_F(AutoGroupModelTest, LookingForPartyOrder) {
	Member& a = addMember("Alpha");
	Member& b = addMember("Bravo", 101.0f);
	Member& c = addMember("Charlie", 102.0f);
	runtime::Ref<LookingForParty> early = solo(c, EntryRequestType::NEW_GROUP_ENTRY);
	std::this_thread::sleep_for(std::chrono::milliseconds(3));
	runtime::Ref<LookingForParty> late = solo(c, EntryRequestType::NEW_GROUP_ENTRY);
	runtime::Ref<LookingForParty> quick = solo(c, EntryRequestType::QUICK_GROUP_ENTRY);
	form({&a, &b});
	runtime::Ref<LookingForParty> pair = solo(a, EntryRequestType::NEW_GROUP_ENTRY);
	EXPECT_EQ(early->compareTo(*quick), 1) << "QUICK (ordinal 1) before NEW (0): lfp.ert - ert";
	EXPECT_EQ(quick->compareTo(*early), -1);
	EXPECT_EQ(late->compareTo(*pair), 1) << "the pair (2 members) before the single player";
	EXPECT_LT(early->compareTo(*late), 0) << "the earlier registration first";
	EXPECT_EQ(early->compareTo(*late), static_cast<int32_t>(early->getRegistrationTime() - late->getRegistrationTime()));
}

// ---- AutoInstance and its registration ---------------------------------------------------------------------------------------------------

/** AutoPvpInstance.java:24-35, :77-84: a side holds the race's member count; the instance is READY when both sides are full */
TEST_F(AutoGroupModelTest, PvpInstanceRegistersBothSides) {
	runtime::Ref<ag::AutoInstance> dredgion = ag::createAutoInstance(AutoGroupType::BARANATH_DREDGION);
	EXPECT_EQ(dredgion->getMaxPlayers(), 4) << "no instance yet: 2 Asmodians + 2 Elyos";
	Member& e1 = addMember("Elyone");
	Member& e2 = addMember("Elytwo", 101.0f);
	Member& e3 = addMember("Elythree", 102.0f);
	Member& a1 = asmodian("Asmone");
	Member& a2 = asmodian("Asmtwo");
	EXPECT_EQ(dredgion->addLookingForParty(*solo(e1)), AGQuestion::ADDED);
	EXPECT_EQ(dredgion->addLookingForParty(*solo(e2)), AGQuestion::ADDED);
	EXPECT_EQ(dredgion->addLookingForParty(*solo(e3)), AGQuestion::FAILED) << "the Elyos side is full";
	EXPECT_EQ(dredgion->addLookingForParty(*solo(a1)), AGQuestion::ADDED);
	EXPECT_EQ(dredgion->addLookingForParty(*solo(a2)), AGQuestion::READY) << "4 of 4 and no instance";
	EXPECT_EQ(dredgion->getRegisteredAGPlayers().size(), 4);
	EXPECT_EQ(dredgion->addLookingForParty(*solo(e3)), AGQuestion::FAILED) << "full";
	dredgion->unregister(e1.player());
	EXPECT_EQ(dredgion->getRegisteredAGPlayers().size(), 3);
	EXPECT_EQ(dredgion->addLookingForParty(*solo(e3)), AGQuestion::READY);
}

/** AutoInstance.java:48-52, :79-90: once created, only a quick entry within the maximum join time may still register */
TEST_F(AutoGroupModelTest, RegistrationAfterTheInstanceStarted) {
	runtime::Ref<ag::AutoInstance> kamar = ag::createAutoInstance(AutoGroupType::KAMAR_BATTLEFIELD);
	Member& e1 = addMember("Elyone");
	runtime::Ref<world::WorldMapInstance> arena = world::WorldMap2DInstance::create(*map, 2, 0, 6, [](world::WorldMapInstance& instance) {
		return runtime::Ref<::aion::gameserver::instance::handlers::InstanceHandler>(
			::aion::gameserver::instance::handlers::GeneralInstanceHandler::create(instance));
	});
	kamar->onInstanceCreate(*arena);
	EXPECT_EQ(kamar->getInstance(), runtime::Ptr<world::WorldMapInstance>(arena));
	EXPECT_NE(kamar->getStartInstanceTime(), 0);
	EXPECT_EQ(kamar->getMaxPlayers(), 6) << "the instance's maximum";
	EXPECT_EQ(kamar->addLookingForParty(*solo(e1, EntryRequestType::NEW_GROUP_ENTRY, 107)), AGQuestion::FAILED) << "started: a new entry is refused";
	EXPECT_EQ(kamar->addLookingForParty(*solo(e1, EntryRequestType::QUICK_GROUP_ENTRY, 107)), AGQuestion::ADDED)
		<< "a quick entry within 230 s; ADDED, not READY, with an instance";
}

/** AutoInstance.java:120-125: before the instance, the first registered player's race picks the cooltime member count (Kamar: 3 / 2) */
TEST_F(AutoGroupModelTest, FfaInstanceTakesSinglePlayers) {
	runtime::Ref<ag::AutoInstance> chaos = ag::createAutoInstance(AutoGroupType::ARENA_OF_CHAOS_1);
	EXPECT_EQ(chaos->getMaxPlayers(), 3) << "nobody registered: the Elyos count";
	Member& a = addMember("Alpha");
	Member& b = addMember("Bravo", 101.0f);
	Member& c = addMember("Charlie", 102.0f);
	Member& d = addMember("Delta", 103.0f);
	form({&c, &d});
	EXPECT_EQ(chaos->addLookingForParty(*solo(c)), AGQuestion::FAILED) << "a party of two";
	EXPECT_EQ(chaos->addLookingForParty(*solo(a)), AGQuestion::ADDED);
	EXPECT_EQ(chaos->addLookingForParty(*solo(b)), AGQuestion::ADDED);
	PlayerGroupService::removePlayer(d.player());
	EXPECT_EQ(chaos->addLookingForParty(*solo(c)), AGQuestion::READY) << "3 of 3";
	EXPECT_EQ(chaos->addLookingForParty(*solo(d)), AGQuestion::FAILED) << "full";
}

/** AutoHarmonyInstance.java:36-45, :104-150: two parties of up to three of one race; leaving takes the player out of his party */
TEST_F(AutoGroupModelTest, HarmonyInstanceFillsTwoParties) {
	runtime::Ref<ag::AutoInstance> harmony = ag::createAutoInstance(AutoGroupType::ARENA_OF_HARMONY_1);
	Member& a = addMember("Alpha");
	Member& b = addMember("Bravo", 101.0f);
	Member& c = addMember("Charlie", 102.0f);
	Member& d = addMember("Delta", 103.0f);
	Member& x = asmodian("Xray");
	Member& y = asmodian("Yankee");
	form({&a, &b, &c});
	EXPECT_EQ(harmony->addLookingForParty(*solo(a)), AGQuestion::ADDED) << "party 0: three Elyos";
	EXPECT_EQ(harmony->addLookingForParty(*solo(d)), AGQuestion::ADDED) << "party 0 is full: party 1";
	EXPECT_EQ(harmony->addLookingForParty(*solo(x)), AGQuestion::FAILED) << "party 0 full, party 1 Elyos";
	harmony->unregister(d.player());
	EXPECT_EQ(harmony->getRegisteredAGPlayers().size(), 3);
	EXPECT_EQ(harmony->addLookingForParty(*solo(x)), AGQuestion::ADDED) << "party 1 is empty again";
	EXPECT_EQ(harmony->addLookingForParty(*solo(y)), AGQuestion::ADDED);
	EXPECT_EQ(harmony->getRegisteredAGPlayers().size(), 5);
}

/** AutoInstance.java:32-47: the items of the id, those that expire first used first; nothing taken when there are too few */
TEST_F(AutoGroupModelTest, RemoveItemTakesTheCount) {
	Member& a = addMember("Alpha");
	runtime::Ref<PvpProbe> probe = PvpProbe::make(AutoGroupType::BARANATH_DREDGION);
	for (int32_t objId : {9301, 9302}) { // two stacks of the juice, 10 each
		runtime::Ref<model::gameobjects::Item> item =
			items::loadedItem(objId, items::MERCENARYS_FRUIT_JUICE, 10, model::items::storage::StorageType::CUBE, objId - 9301);
		a.player().getStorage(model::items::storage::getId(model::items::storage::StorageType::CUBE))->onLoadHandler(*item);
		storedItems.push_back(item);
	}
	EXPECT_FALSE(probe->removeItem(a.player(), items::MERCENARYS_FRUIT_JUICE, 21)) << "20 of 21";
	EXPECT_EQ(a.player().getInventory().getItemCountByItemId(items::MERCENARYS_FRUIT_JUICE), 20);
	EXPECT_TRUE(probe->removeItem(a.player(), items::MERCENARYS_FRUIT_JUICE, 15));
	EXPECT_EQ(a.player().getInventory().getItemCountByItemId(items::MERCENARYS_FRUIT_JUICE), 5) << "15 across both stacks, no more";
	EXPECT_TRUE(probe->removeItem(a.player(), items::MERCENARYS_FRUIT_JUICE, 5));
	EXPECT_EQ(a.player().getInventory().getItemCountByItemId(items::MERCENARYS_FRUIT_JUICE), 0);
}

/** AutoPvPFFAInstance.java:37-56: without the arena ticket the player leaves the registration (window 5) */
TEST_F(AutoGroupModelTest, FfaEntryWithoutTicket) {
	runtime::Ref<ag::AutoInstance> chaos = ag::createAutoInstance(AutoGroupType::ARENA_OF_CHAOS_1);
	Member& a = addMember("Alpha");
	Member& b = addMember("Bravo", 101.0f);
	chaos->addLookingForParty(*solo(a));
	chaos->addLookingForParty(*solo(b));
	a.clearSent();
	chaos->onPressEnter(a.player());
	EXPECT_EQ(a.count(SM_AUTO_GROUP(21, 5)), 1);
	EXPECT_EQ(chaos->getRegisteredAGPlayers().size(), 1) << "Bravo stays: the instance is not destroyed";
}

// ---- AutoGroupUtility -----------------------------------------------------------------------------------------------------------------------

/** AutoGroupUtility.java:20-38, :92-104: the template's entry flags; a player in a team must use the group entry */
TEST_F(AutoGroupModelTest, UtilityEntryChecks) {
	Member& a = addMember("Alpha");
	Member& b = addMember("Bravo", 101.0f);
	EXPECT_TRUE(AutoGroupUtility::canRegisterNewEntry(a.player(), AutoGroupType::ARENA_OF_DISCIPLINE_1));
	EXPECT_FALSE(AutoGroupUtility::canRegisterQuickEntry(a.player(), AutoGroupType::ARENA_OF_DISCIPLINE_1)) << "mask 24 has no register_quick";
	EXPECT_TRUE(AutoGroupUtility::canRegisterQuickEntry(a.player(), AutoGroupType::ARENA_OF_CHAOS_1));
	EXPECT_FALSE(AutoGroupUtility::canRegisterGroupEntry(a.player(), AutoGroupType::ARENA_OF_CHAOS_1, 300350000, 21)) << "no register_group";
	form({&a, &b});
	EXPECT_FALSE(AutoGroupUtility::canRegisterNewEntry(a.player(), AutoGroupType::ARENA_OF_CHAOS_1));
	EXPECT_EQ(a.count(SM_SYSTEM_MESSAGE::STR_MSG_CANT_INSTANCE_NOT_LEADER()), 1);
	EXPECT_FALSE(AutoGroupUtility::hasCoolDown(a.player(), 300350000));
	cooldown(a, 300350000);
	EXPECT_TRUE(AutoGroupUtility::hasCoolDown(a.player(), 300350000));
	a.clearSent();
	AutoGroupUtility::sendWindowToPlayer(a.player(), 21, 3);
	EXPECT_EQ(a.count(SM_AUTO_GROUP(21, 3)), 1);
}

/** AutoGroupUtility.java:44-78: the leader, the team size of the instance, and every member's cooldown and level */
TEST_F(AutoGroupModelTest, UtilityGroupRequirements) {
	Member& a = addMember("Alpha");
	Member& b = addMember("Bravo", 101.0f);
	Member& c = addMember("Charlie", 102.0f);
	Member& d = addMember("Delta", 103.0f);
	EXPECT_FALSE(AutoGroupUtility::checkGroupRequirements(a.player(), AutoGroupType::KAMAR_BATTLEFIELD, 301120000, 107));
	EXPECT_EQ(a.count(SM_SYSTEM_MESSAGE::STR_MSG_CANT_INSTANCE_NOT_LEADER()), 1) << "no team";
	form({&a, &b, &c, &d});
	a.clearSent();
	EXPECT_FALSE(AutoGroupUtility::checkGroupRequirements(b.player(), AutoGroupType::KAMAR_BATTLEFIELD, 301120000, 107));
	EXPECT_EQ(b.count(SM_SYSTEM_MESSAGE::STR_MSG_CANT_INSTANCE_NOT_LEADER()), 1) << "not the leader";
	EXPECT_FALSE(AutoGroupUtility::checkGroupRequirements(a.player(), AutoGroupType::KAMAR_BATTLEFIELD, 301120000, 107));
	EXPECT_EQ(a.count(SM_SYSTEM_MESSAGE::STR_MSG_CANT_INSTANCE_TOO_MANY_MEMBERS(3, 301120000)), 1) << "periodic: 4 > Kamar's 3 Elyos";
	EXPECT_FALSE(AutoGroupUtility::checkGroupRequirements(a.player(), AutoGroupType::HARAMONIOUS_TRAINING_CENTER_1, 300450000, 101));
	EXPECT_EQ(a.count(SM_SYSTEM_MESSAGE::STR_MSG_CANT_INSTANCE_TOO_MANY_MEMBERS(3, 300450000)), 1) << "harmony: 4 > 3";
	PlayerGroupService::removePlayer(d.player());
	a.clearSent();
	EXPECT_FALSE(AutoGroupUtility::checkGroupRequirements(a.player(), AutoGroupType::BARANATH_DREDGION, 300110000, 1));
	EXPECT_EQ(a.count(SM_SYSTEM_MESSAGE::STR_MSG_CANT_INSTANCE_TOO_MANY_MEMBERS(2, 300110000)), 1) << "periodic: 3 > the dredgion's 2";
	a.clearSent();
	// mask 22's test row is for level 1: only the cooldown refuses (without it the check would go on to AutoGroupService.isSearching, CP2)
	cooldown(b, 390000022);
	cooldown(c, 390000022);
	EXPECT_FALSE(AutoGroupUtility::checkGroupRequirements(a.player(), AutoGroupType::ARENA_OF_CHAOS_2, 390000022, 22));
	EXPECT_EQ(a.count(SM_SYSTEM_MESSAGE::STR_MSG_CANT_INSTANCE_ENTER_MEMBER("Bravo")) + a.count(SM_SYSTEM_MESSAGE::STR_MSG_CANT_INSTANCE_ENTER_MEMBER("Charlie")), 1)
		<< "the first member after the leader: his cooldown";
	a.clearSent();
	EXPECT_FALSE(AutoGroupUtility::checkGroupRequirements(a.player(), AutoGroupType::ARENA_OF_DISCIPLINE_1, 300360000, 24));
	EXPECT_EQ(a.count(SM_SYSTEM_MESSAGE::STR_MSG_CANT_INSTANCE_ENTER_MEMBER("Bravo")) + a.count(SM_SYSTEM_MESSAGE::STR_MSG_CANT_INSTANCE_ENTER_MEMBER("Charlie")), 1)
		<< "no cooldown there: level 1 is out of 46-50";
}

/** AutoGroupUtility.java:80-90: every online member: the periodic instance's window 6, the success message and window 1 with the leader */
TEST_F(AutoGroupModelTest, UtilitySuccessfulRegistration) {
	Member& a = addMember("Alpha");
	Member& b = addMember("Bravo", 101.0f);
	form({&a, &b});
	world::World::getInstance().storeObject(a.player());
	world::World::getInstance().storeObject(b.player());
	runtime::Ref<LookingForParty> party = solo(a, EntryRequestType::GROUP_ENTRY, 107);
	AutoGroupUtility::sendSuccessfulRegistration(*party, "Alpha", AutoGroupType::KAMAR_BATTLEFIELD, 107);
	for (Member* m : {&a, &b}) {
		EXPECT_EQ(m->count(SM_AUTO_GROUP(107, 6, true)), 1);
		EXPECT_EQ(m->count(SM_SYSTEM_MESSAGE::STR_MSG_INSTANCE_REGISTER_SUCCESS()), 1);
		EXPECT_EQ(m->count(SM_AUTO_GROUP(107, 1, 2, "Alpha")), 1);
	}
	a.clearSent();
	AutoGroupUtility::sendSuccessfulRegistration(*party, "Alpha", AutoGroupType::ARENA_OF_CHAOS_1, 21);
	EXPECT_EQ(a.count(SM_AUTO_GROUP(21, 6, true)), 0) << "not periodic";
	AutoGroupUtility::sendWindowToPlayerIfOnline(b.player().getObjectId(), 21, 4);
	EXPECT_EQ(b.count(SM_AUTO_GROUP(21, 4)), 1);
	world::World::getInstance().removeObject(a.player());
	world::World::getInstance().removeObject(b.player());
}

} // namespace
} // namespace aion::gameserver::network::aion::clientpackets::testing::team
