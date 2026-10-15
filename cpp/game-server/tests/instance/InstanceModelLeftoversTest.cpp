// P5-13, M5f (leftovers sweep 2026-10-14): DredgionRoom (DredgionRoom.java) and StageType's constructor data (StageType.java:8-98, 100-111).

#include <gtest/gtest.h>

#include "aion/gameserver/model/Race.h"
#include "aion/gameserver/model/instance/DredgionRoom.h"
#include "aion/gameserver/model/instance/StageTypeInfo.h"

namespace aion::gameserver::model::instance {
namespace {

TEST(DredgionRoomTest, ANewRoomIsUncapturedAndKeepsItsId) {
	runtime::Ref<DredgionRoom> room = DredgionRoom::create(13);
	EXPECT_EQ(room->getRoomId(), 13);
	EXPECT_EQ(room->getState(), 0xFF);
}

TEST(DredgionRoomTest, CaptureSetsOneForAsmodiansAndZeroOtherwise) {
	runtime::Ref<DredgionRoom> room = DredgionRoom::create(1);
	room->captureRoom(Race::ASMODIANS);
	EXPECT_EQ(room->getState(), 0x01);
	room->captureRoom(Race::ELYOS);
	EXPECT_EQ(room->getState(), 0x00);
	room->captureRoom(Race::ASMODIANS);
	room->captureRoom(Race::NPC); // race == ASMODIANS ? 1 : 0
	EXPECT_EQ(room->getState(), 0x00);
}

TEST(StageTypeTest, ConstructorData) {
	EXPECT_EQ(getId(StageType::DEFAULT), 0);
	EXPECT_EQ(getType(StageType::DEFAULT), 0);
	EXPECT_EQ(getId(StageType::START_STAGE_1_ELEVATOR), 35464);
	EXPECT_EQ(getType(StageType::START_STAGE_1_ELEVATOR), 1);
	EXPECT_EQ(getId(StageType::START_STAGE_4_ROUND_1), 38465);
	EXPECT_EQ(getId(StageType::START_STAGE_5), 8392);
	EXPECT_EQ(getId(StageType::PVP_STAGE_6), 6);
	EXPECT_EQ(getId(StageType::PVP_STAGE_OVER), 0);
	EXPECT_EQ(getType(StageType::START_STAGE_5), 3);
}

} // namespace
} // namespace aion::gameserver::model::instance
