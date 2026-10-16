// TribeRelationService (m5a-plan.md F-03, the world flow: Npc.getType for SM_NPC_INFO, aggro checks): the hard-coded tribe switches and the
// fallbacks to TribeRelationsData, with creatures whose tribe and base tribe are given directly (a Creature test double that skips the AI) and
// a small tribe_relations document. Expectations derived by hand from TribeRelationService.java and TribeRelationsData.java. The Player
// (panesterra faction) and Npc (siege relation) branches need those classes and are not part of M5a.

#include <gtest/gtest.h>

#include <optional>
#include <string>

#include "aion/gameserver/controllers/CreatureController.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/TribeRelationsData.bind.h"
#include "aion/gameserver/dataholders/TribeRelationsData.h"
#include "aion/gameserver/dataholders/loadingutils/LoadContext.h"
#include "aion/gameserver/dataholders/loadingutils/StaticDataLoader.h"
#include "aion/gameserver/model/TribeClass.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/base/TaskInfo.h"
#include "aion/gameserver/runtime/lifetime/Reclaimer.h"
#include "aion/gameserver/runtime/lifetime/TaskScope.h"
#include "aion/gameserver/services/TribeRelationService.h"

namespace aion::gameserver::services {
namespace {

using model::TribeClass;
using model::gameobjects::Creature;

/** A creature with a given tribe and base tribe (no controller, no AI: postConstruct is skipped) */
class TribeCreature final : public Creature {
	AION_MAKE_REF_FRIEND

public:
	TribeCreature(CreateKey key, std::optional<TribeClass> tribe, TribeClass baseTribe)
		: Creature(key, 0, nullptr, nullptr, nullptr, nullptr, false), tribe(tribe), baseTribe(baseTribe) {}

	std::optional<TribeClass> getTribe() override { return tribe; }
	TribeClass getBaseTribe() override { return baseTribe; }
	int8_t getLevel() override { return 1; }
	std::string getName() override { return "tribe creature"; }

protected:
	~TribeCreature() override = default;
	void postConstruct() override {}

private:
	const std::optional<TribeClass> tribe;
	const TribeClass baseTribe;
};

const char* RELATIONS = R"(<tribe_relations>)"
						R"(<tribe name="MONSTER"/><tribe name="PC"/><tribe name="PC_DARK"/><tribe name="GUARD"/><tribe name="GENERAL"/>)"
						R"(<tribe name="KRALL" base="MONSTER"><aggro>PC</aggro><hostile>GUARD</hostile></tribe>)"
						R"(<tribe name="DUMMY_LGUARD" base="GUARD"><friend>KRALL_PC</friend></tribe>)"
						R"(<tribe name="KRALL_PC" base="PC"><neutral>DUMMY</neutral><none>DUMMY2</none></tribe>)"
						R"(<tribe name="DUMMY" base="GENERAL"><support>DUMMY2</support></tribe>)"
						R"(<tribe name="DUMMY2" base="GENERAL"/>)"
						R"(</tribe_relations>)";

class TribeRelationServiceTest : public testing::Test {
protected:
	void SetUp() override {
		dataholders::DataManager::TRIBE_RELATIONS_DATA.resetForTests();
		xml::LoadContext context;
		dataholders::DataManager::TRIBE_RELATIONS_DATA.publish(xml::bindString<dataholders::TribeRelationsData>(context, RELATIONS));
	}

	void TearDown() override {
		dataholders::DataManager::TRIBE_RELATIONS_DATA.resetForTests();
		runtime::Reclaimer::getInstance().drain();
	}

	static runtime::Ref<TribeCreature> creature(std::optional<TribeClass> tribe, TribeClass base) {
		return model::gameobjects::VisibleObject::create<TribeCreature>(tribe, base);
	}
};

TEST_F(TribeRelationServiceTest, IsAggressive) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	// the base tribe switches
	EXPECT_TRUE(TribeRelationService::isAggressive(*creature(TribeClass::GUARD_DARK, TribeClass::GUARD_DARK), *creature(TribeClass::PC, TribeClass::PC)));
	EXPECT_TRUE(TribeRelationService::isAggressive(*creature(TribeClass::GUARD, TribeClass::GUARD), *creature(TribeClass::GUARD_DRAGON, TribeClass::GUARD_DRAGON)));
	EXPECT_TRUE(TribeRelationService::isAggressive(*creature(TribeClass::GUARD_DRAGON, TribeClass::GUARD_DRAGON), *creature(TribeClass::GENERAL, TribeClass::GENERAL)));
	EXPECT_FALSE(TribeRelationService::isAggressive(*creature(TribeClass::GUARD, TribeClass::GUARD), *creature(TribeClass::PC, TribeClass::PC)));
	// the tribe switches
	EXPECT_TRUE(TribeRelationService::isAggressive(*creature(TribeClass::AGGRESSIVESINGLEMONSTER, TribeClass::MONSTER),
		*creature(TribeClass::YUN_GUARD, TribeClass::GUARD)));
	EXPECT_FALSE(TribeRelationService::isAggressive(*creature(TribeClass::IDF5U2_SHULACK, TribeClass::GUARD_DARK),
		*creature(TribeClass::FIELD_OBJECT_ALL_HOSTILEMONSTER, TribeClass::PC)));
	// the relations data: KRALL's aggro list names KRALL_PC's base PC
	EXPECT_TRUE(TribeRelationService::isAggressive(*creature(TribeClass::KRALL, TribeClass::MONSTER), *creature(TribeClass::KRALL_PC, TribeClass::PC)));
	EXPECT_TRUE(TribeRelationService::isAggressive(*creature(TribeClass::KRALL_PC, TribeClass::PC), *creature(TribeClass::KRALL, TribeClass::MONSTER)));
	EXPECT_FALSE(TribeRelationService::isAggressive(*creature(TribeClass::KRALL, TribeClass::MONSTER), *creature(TribeClass::DUMMY, TribeClass::GENERAL)));
	// a null tribe: Java's switch throws, the relation of an unknown tribe is false
	EXPECT_THROW(TribeRelationService::isAggressive(*creature(std::nullopt, TribeClass::MONSTER), *creature(TribeClass::PC, TribeClass::PC)),
		runtime::NullPointerException);
	EXPECT_FALSE(TribeRelationService::isAggressive(*creature(TribeClass::KRALL, TribeClass::MONSTER), *creature(std::nullopt, TribeClass::PC)));
}

TEST_F(TribeRelationServiceTest, IsFriend) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	EXPECT_TRUE(TribeRelationService::isFriend(*creature(TribeClass::DUMMY, TribeClass::GENERAL), *creature(TribeClass::DUMMY, TribeClass::PC_DARK)));
	EXPECT_TRUE(TribeRelationService::isFriend(*creature(std::nullopt, TribeClass::MONSTER), *creature(std::nullopt, TribeClass::PC))); // null == null
	EXPECT_TRUE(TribeRelationService::isFriend(*creature(TribeClass::IDF5U2_SHULACK, TribeClass::MONSTER),
		*creature(TribeClass::FIELD_OBJECT_ALL_HOSTILEMONSTER, TribeClass::MONSTER)));
	EXPECT_TRUE(TribeRelationService::isFriend(*creature(TribeClass::DUMMY2, TribeClass::USEALL), *creature(TribeClass::KRALL, TribeClass::MONSTER)));
	EXPECT_TRUE(TribeRelationService::isFriend(*creature(TribeClass::DUMMY2, TribeClass::GENERAL), *creature(TribeClass::PC, TribeClass::PC)));
	EXPECT_FALSE(TribeRelationService::isFriend(*creature(TribeClass::DRAMA_EVE_NONPC_A, TribeClass::GENERAL), *creature(TribeClass::PC, TribeClass::PC)));
	EXPECT_TRUE(TribeRelationService::isFriend(*creature(TribeClass::DUMMY2, TribeClass::FIELD_OBJECT_DARK), *creature(TribeClass::PC_DARK, TribeClass::PC_DARK)));
	EXPECT_FALSE(TribeRelationService::isFriend(*creature(TribeClass::DUMMY2, TribeClass::FIELD_OBJECT_DARK), *creature(TribeClass::PC, TribeClass::PC)));
	// the relations data: DUMMY_LGUARD's friend list names KRALL_PC
	EXPECT_TRUE(TribeRelationService::isFriend(*creature(TribeClass::DUMMY_LGUARD, TribeClass::GUARD), *creature(TribeClass::KRALL_PC, TribeClass::PC)));
}

TEST_F(TribeRelationServiceTest, IsSupportIsNoneIsNeutralIsHostile) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	EXPECT_TRUE(TribeRelationService::isSupport(*creature(TribeClass::DUMMY, TribeClass::GENERAL), *creature(TribeClass::DUMMY2, TribeClass::GENERAL)));
	EXPECT_TRUE(TribeRelationService::isSupport(*creature(TribeClass::GENERAL, TribeClass::MONSTER), *creature(TribeClass::DUMMY2, TribeClass::GENERAL)));
	EXPECT_TRUE(TribeRelationService::isSupport(*creature(TribeClass::DUMMY_LGUARD, TribeClass::GUARD), *creature(TribeClass::KRALL_PC, TribeClass::PC)));
	EXPECT_FALSE(TribeRelationService::isSupport(*creature(TribeClass::KRALL, TribeClass::MONSTER), *creature(TribeClass::KRALL_PC, TribeClass::PC)));

	// isNone: false for aggressive, hostile or neutral relations; then the base tribe switch; then the none relation
	EXPECT_FALSE(TribeRelationService::isNone(*creature(TribeClass::DUMMY, TribeClass::GENERAL), *creature(TribeClass::KRALL_PC, TribeClass::PC_DARK)));
	EXPECT_TRUE(TribeRelationService::isNone(*creature(TribeClass::DUMMY2, TribeClass::GENERAL), *creature(TribeClass::KRALL_PC, TribeClass::PC_DARK)));
	EXPECT_TRUE(TribeRelationService::isNone(*creature(TribeClass::DUMMY2, TribeClass::GAB1_PEACE), *creature(TribeClass::KRALL, TribeClass::MONSTER)));
	EXPECT_FALSE(TribeRelationService::isNone(*creature(TribeClass::KRALL, TribeClass::GAB1_PEACE), *creature(TribeClass::KRALL_PC, TribeClass::PC)));
	EXPECT_TRUE(TribeRelationService::isNone(*creature(TribeClass::KRALL_PC, TribeClass::PC), *creature(TribeClass::DUMMY2, TribeClass::GENERAL)));
	EXPECT_FALSE(TribeRelationService::isNone(*creature(TribeClass::DUMMY2, TribeClass::MONSTER), *creature(TribeClass::DUMMY, TribeClass::GENERAL)));

	EXPECT_TRUE(TribeRelationService::isNeutral(*creature(TribeClass::DUMMY, TribeClass::GENERAL), *creature(TribeClass::KRALL_PC, TribeClass::PC)));
	EXPECT_FALSE(TribeRelationService::isNeutral(*creature(TribeClass::DUMMY2, TribeClass::GENERAL), *creature(TribeClass::KRALL_PC, TribeClass::PC)));

	EXPECT_TRUE(TribeRelationService::isHostile(*creature(TribeClass::DUMMY, TribeClass::MONSTER), *creature(TribeClass::PC_DARK, TribeClass::PC_DARK)));
	EXPECT_FALSE(TribeRelationService::isHostile(*creature(TribeClass::IDF5U2_SHULACK, TribeClass::MONSTER),
		*creature(TribeClass::FIELD_OBJECT_ALL_HOSTILEMONSTER, TribeClass::PC)));
	// the relations data: KRALL's hostile list names DUMMY_LGUARD's base GUARD
	EXPECT_TRUE(TribeRelationService::isHostile(*creature(TribeClass::KRALL, TribeClass::MONSTER), *creature(TribeClass::DUMMY_LGUARD, TribeClass::GUARD)));
	EXPECT_FALSE(TribeRelationService::isHostile(*creature(TribeClass::DUMMY, TribeClass::GENERAL), *creature(TribeClass::PC, TribeClass::PC)));

	EXPECT_TRUE(TribeRelationService::canHelpCreature(*creature(TribeClass::DUMMY, TribeClass::GENERAL), *creature(TribeClass::DUMMY2, TribeClass::GENERAL)));
	EXPECT_FALSE(TribeRelationService::canHelpCreature(*creature(TribeClass::DUMMY2, TribeClass::GENERAL), *creature(TribeClass::DUMMY, TribeClass::GENERAL)));
	EXPECT_FALSE(TribeRelationService::canHelpCreature(*creature(std::nullopt, TribeClass::GENERAL), *creature(TribeClass::DUMMY2, TribeClass::GENERAL)));
}

} // namespace
} // namespace aion::gameserver::services
