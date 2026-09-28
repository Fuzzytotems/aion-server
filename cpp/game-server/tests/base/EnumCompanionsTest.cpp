// P4-05 enum companions (docs/design/static-data.md §2.5): Java constructor data and methods of the model enums, with expectations read from the
// Java enums (model/*.java, model/animations/*.java).

#include <gtest/gtest.h>

#include <optional>
#include <string>

#include "aion/gameserver/model/ActionStateInfo.h"
#include "aion/gameserver/model/AttendTypeInfo.h"
#include "aion/gameserver/model/ChatTypeInfo.h"
#include "aion/gameserver/model/CreatureTypeInfo.h"
#include "aion/gameserver/model/DialogPageInfo.h"
#include "aion/gameserver/model/DuelResultInfo.h"
#include "aion/gameserver/model/EmotionIdInfo.h"
#include "aion/gameserver/model/EmotionTypeInfo.h"
#include "aion/gameserver/model/EventThemeInfo.h"
#include "aion/gameserver/model/GenderInfo.h"
#include "aion/gameserver/model/PlayerClassInfo.h"
#include "aion/gameserver/model/RaceInfo.h"
#include "aion/gameserver/model/SellLimitInfo.h"
#include "aion/gameserver/model/SkillElementInfo.h"
#include "aion/gameserver/model/TribeClassInfo.h"
#include "aion/gameserver/model/animations/ActionAnimationInfo.h"
#include "aion/gameserver/model/animations/ArrivalAnimationInfo.h"
#include "aion/gameserver/model/animations/AttackHandAnimationInfo.h"
#include "aion/gameserver/model/animations/AttackTypeAnimationInfo.h"
#include "aion/gameserver/model/animations/ObjectDeleteAnimationInfo.h"
#include "aion/gameserver/model/animations/TeleportAnimationInfo.h"
#include "aion/gameserver/runtime/base/Exceptions.h"

namespace aion::gameserver::model {
namespace {

TEST(EnumCompanionsTest, ChatTypeIdsAndLookup) {
	EXPECT_EQ(getId(ChatType::NORMAL), 0);
	EXPECT_EQ(getId(ChatType::SHOUT), 3);
	EXPECT_EQ(getId(ChatType::CH1), 14);
	EXPECT_EQ(getId(ChatType::GOLDEN_YELLOW), 25);
	EXPECT_EQ(getId(ChatType::GM_CHAT), 27);
	EXPECT_EQ(getId(ChatType::BRIGHT_YELLOW_CENTER), 36);
	EXPECT_TRUE(isSysMsg(ChatType::GOLDEN_YELLOW));
	EXPECT_FALSE(isSysMsg(ChatType::GM_CHAT));
	EXPECT_TRUE(isSysMsg(ChatType::WHITE));
	EXPECT_FALSE(isSysMsg(ChatType::COMMAND));
	EXPECT_EQ(getChatType(36), ChatType::BRIGHT_YELLOW_CENTER);
	EXPECT_EQ(getChatType(10), ChatType::LEGION);
	try {
		getChatType(2);
		FAIL() << "expected IllegalArgumentException";
	} catch (const runtime::IllegalArgumentException& e) {
		EXPECT_STREQ(e.what(), "Unsupported chat type: 2");
	}
	try {
		getChatType(-1); // Java: (id & 0xFF)
		FAIL() << "expected IllegalArgumentException";
	} catch (const runtime::IllegalArgumentException& e) {
		EXPECT_STREQ(e.what(), "Unsupported chat type: 255");
	}
}

TEST(EnumCompanionsTest, RaceData) {
	EXPECT_EQ(getRaceId(Race::ELYOS), 0);
	EXPECT_EQ(getL10nId(Race::ASMODIANS), 900241);
	EXPECT_EQ(getL10nId(Race::LYCAN), 0);
	EXPECT_EQ(getRaceId(Race::ELEMENTAL), 25);
	EXPECT_EQ(getRaceId(Race::LIVINGWATER), 28); // Java declares LIVINGWATER(28) and DEFORM(28)
	EXPECT_EQ(getRaceId(Race::NONE), 26);
	EXPECT_EQ(getRaceId(Race::DEFORM), 28);
	EXPECT_EQ(getRaceId(Race::LF5_Q_ITEM), 46);
	EXPECT_TRUE(isAsmoOrEly(Race::ASMODIANS));
	EXPECT_FALSE(isAsmoOrEly(Race::PC_ALL));
	EXPECT_TRUE(isPlayerRace(Race::PC_ALL));
	EXPECT_FALSE(isPlayerRace(Race::NPC));
	EXPECT_EQ(getRaceByString("NPC"), Race::NPC);
	EXPECT_EQ(getRaceByString("npc"), std::nullopt);
	EXPECT_EQ(getRaceByString("ELYOS"), Race::ELYOS);
}

TEST(EnumCompanionsTest, PlayerClassData) {
	EXPECT_EQ(getClassId(PlayerClass::SORCERER), 7);
	EXPECT_EQ(getPlayerClassById(7), PlayerClass::SORCERER);
	EXPECT_EQ(getPlayerClassById(16, true), PlayerClass::BARD);
	EXPECT_EQ(getPlayerClassById(17, true), std::nullopt);
	try {
		getPlayerClassById(17);
		FAIL() << "expected IllegalArgumentException";
	} catch (const runtime::IllegalArgumentException& e) {
		EXPECT_STREQ(e.what(), "There is no player class with id 17");
	}
	EXPECT_EQ(getL10nId(PlayerClass::GUNNER), 904316);
	EXPECT_TRUE(isStartingClass(PlayerClass::MAGE));
	EXPECT_FALSE(isStartingClass(PlayerClass::CLERIC));
	EXPECT_EQ(getStartingClass(PlayerClass::BARD), PlayerClass::ARTIST);
	EXPECT_EQ(getStartingClass(PlayerClass::WARRIOR), PlayerClass::WARRIOR);
	EXPECT_TRUE(isPhysicalClass(PlayerClass::CHANTER));
	EXPECT_FALSE(isPhysicalClass(PlayerClass::CLERIC));
	EXPECT_EQ(getIconImage(PlayerClass::PRIEST), "textures/ui/EMBLEM/icon_emblem_cleric.dds");
	EXPECT_EQ(getIconImage(PlayerClass::RIDER), "textures/ui/EMBLEM/Icon_emblem_Rider.dds");
	EXPECT_EQ(getPower(PlayerClass::TEMPLAR), 115);
	EXPECT_EQ(getWill(PlayerClass::TEMPLAR), 105);
	EXPECT_EQ(getHealthMultiplier(PlayerClass::TEMPLAR), 460);
	EXPECT_EQ(getWillMultiplier(PlayerClass::BARD), 520);
	EXPECT_EQ(getMagicalCriticalResist(PlayerClass::SPIRIT_MASTER), 50);
	EXPECT_EQ(getAgilityMultiplier(PlayerClass::BARD), 310);
	EXPECT_EQ(getAccuracyMultiplier(PlayerClass::BARD), 200);
	EXPECT_EQ(getNoWeaponPowerMultiplier(PlayerClass::BARD), 70);
}

TEST(EnumCompanionsTest, EmotionTypeAndEmotionId) {
	EXPECT_EQ(getTypeId(EmotionType::NONE), -1);
	EXPECT_EQ(getTypeId(EmotionType::RESURRECT), 19);
	EXPECT_EQ(getTypeId(EmotionType::EMOTE), 21);
	EXPECT_EQ(getTypeId(EmotionType::END_SPRINT), 54);
	EXPECT_EQ(getEmotionTypeById(21), EmotionType::EMOTE);
	EXPECT_EQ(getEmotionTypeById(31), EmotionType::OPEN_DOOR);
	EXPECT_EQ(getEmotionTypeById(20), EmotionType::NONE);
	EXPECT_EQ(getEmotionTypeById(-1), EmotionType::NONE);
	EXPECT_EQ(id(EmotionId::STAND), 128);
	EXPECT_EQ(id(EmotionId::SMILE), 28);
}

TEST(EnumCompanionsTest, DialogPageData) {
	EXPECT_EQ(id(DialogPage::NULL_), 0);
	EXPECT_EQ(id(DialogPage::DEPOSIT_CHAR_WAREHOUSE), 26);
	EXPECT_EQ(id(DialogPage::SELECT_QUEST_REWARD_WINDOW5), 45);
	EXPECT_EQ(getByActionId(DialogAction::OPEN_VENDOR), DialogPage::VENDOR);
	EXPECT_EQ(getByActionId(DialogAction::NULL_), DialogPage::NULL_);
	// the pages created with the one-argument constructor have dialogActionId 0; the first of them is found
	EXPECT_EQ(getByActionId(0), DialogPage::ASK_QUEST_ACCEPT_WINDOW);
	EXPECT_EQ(getByActionId(-12345), DialogPage::NULL_);
	EXPECT_EQ(getRewardPageByIndex(std::nullopt), DialogPage::NULL_);
	EXPECT_EQ(getRewardPageByIndex(0), DialogPage::SELECT_QUEST_REWARD_WINDOW1);
	EXPECT_EQ(getRewardPageByIndex(9), DialogPage::SELECT_QUEST_REWARD_WINDOW10);
	EXPECT_EQ(getRewardPageByIndex(10), DialogPage::NULL_);
}

TEST(EnumCompanionsTest, SmallEnums) {
	EXPECT_EQ(getL10nId(ActionState::STANDING), 1400053);
	EXPECT_EQ(getL10nId(ActionState::COMBAT), 1400079);
	EXPECT_EQ(getL10nId(ActionState::POLYMORPH), 1401212);
	EXPECT_EQ(getId(AttendType::CUMULATIVE), 2);
	EXPECT_EQ(getId(CreatureType::FRIEND), 38);
	EXPECT_EQ(getId(CreatureType::SUPPORT), 54);
	EXPECT_EQ(getMsgId(DuelResult::DUEL_LOST), 1300099);
	EXPECT_EQ(getResultId(DuelResult::DUEL_WON), 2);
	EXPECT_EQ(getId(EventTheme::BRAXCAFE), 8);
	EXPECT_EQ(getId(EventTheme::TEST_BASIC_4), 128);
	EXPECT_EQ(getGenderId(Gender::FEMALE), 1);
	EXPECT_EQ(getStatForElement(SkillElement::NONE), std::nullopt);
	EXPECT_EQ(getStatForElement(SkillElement::WIND), stats::container::StatEnum::WIND_RESISTANCE);
	EXPECT_TRUE(isGuard(TribeClass::BROWNIEGUARD));
	EXPECT_TRUE(isGuard(TribeClass::BMDGUARDIAN)); // contains("GUARD")
	EXPECT_FALSE(isGuard(TribeClass::PC));
	EXPECT_TRUE(isPC(TribeClass::PC_DARK));
	EXPECT_FALSE(isPC(TribeClass::BROWNIEGUARD));
	EXPECT_EQ(detail::SELL_LIMIT_DATA[static_cast<size_t>(SellLimit::LIMIT_41_55)].limit, 12050047);
}

TEST(EnumCompanionsTest, Animations) {
	using namespace animations;
	EXPECT_EQ(getId(ActionAnimation::CLASS_CHANGE), 4);
	EXPECT_EQ(getId(ArrivalAnimation::LANDING_GLOW), 18);
	EXPECT_EQ(getId(AttackHandAnimation::RANDOM), 2);
	EXPECT_EQ(getId(AttackTypeAnimation::RANGED), 1);
	EXPECT_EQ(getId(ObjectDeleteAnimation::DELAYED), 19);
	EXPECT_EQ(getId(TeleportAnimation::JUMP_IN_GATE), 8);
	EXPECT_EQ(getId(TeleportAnimation::BATTLEGROUND), 0);
	EXPECT_EQ(getDefaultArrivalAnimation(TeleportAnimation::FADE_OUT_BEAM), ArrivalAnimation::FADE_IN_BEAM);
	EXPECT_EQ(getDefaultArrivalAnimation(TeleportAnimation::JUMP_IN_STATUE), ArrivalAnimation::JUMP_OUT_CAMERA_FRONT);
	EXPECT_EQ(getDefaultArrivalAnimation(TeleportAnimation::JUMP_IN_GATE), ArrivalAnimation::JUMP_OUT_CAMERA_BEHIND);
	EXPECT_EQ(getDefaultArrivalAnimation(TeleportAnimation::BATTLEGROUND), ArrivalAnimation::LANDING_GLOW);
	EXPECT_EQ(getDefaultArrivalAnimation(TeleportAnimation::NONE), ArrivalAnimation::LANDING);
	EXPECT_EQ(getDefaultObjectDeleteAnimation(TeleportAnimation::JUMP_IN_STATUE), ObjectDeleteAnimation::JUMP_IN);
	EXPECT_EQ(getDefaultObjectDeleteAnimation(TeleportAnimation::FADE_OUT_BEAM), ObjectDeleteAnimation::FADE_OUT_BEAM);
	EXPECT_EQ(getDefaultObjectDeleteAnimation(TeleportAnimation::BATTLEGROUND), ObjectDeleteAnimation::FADE_OUT);
}

} // namespace
} // namespace aion::gameserver::model
