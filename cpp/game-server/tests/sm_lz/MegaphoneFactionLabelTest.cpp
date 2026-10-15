// P4-17 (the leftovers sweep, 2026-10-14): SM_MEGAPHONE.FactionLabel's constructor data (SM_MEGAPHONE.java:39-48): NONE is (byte) -1,
// ELYOS and ASMODIANS are their races' ids.

#include <gtest/gtest.h>

#include "aion/gameserver/model/Race.h"
#include "aion/gameserver/model/RaceInfo.h"
#include "aion/gameserver/network/aion/serverpackets/SM_MEGAPHONE_FactionLabelInfo.h"

namespace aion::gameserver::network::aion::serverpackets {
namespace {

TEST(MegaphoneFactionLabelTest, Ids) {
	EXPECT_EQ(id(SM_MEGAPHONE_FactionLabel::NONE), -1);
	EXPECT_EQ(id(SM_MEGAPHONE_FactionLabel::ELYOS), static_cast<int8_t>(model::getRaceId(model::Race::ELYOS)));
	EXPECT_EQ(id(SM_MEGAPHONE_FactionLabel::ASMODIANS), static_cast<int8_t>(model::getRaceId(model::Race::ASMODIANS)));
	EXPECT_NE(id(SM_MEGAPHONE_FactionLabel::ELYOS), id(SM_MEGAPHONE_FactionLabel::ASMODIANS));
}

} // namespace
} // namespace aion::gameserver::network::aion::serverpackets
