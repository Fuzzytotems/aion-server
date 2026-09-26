// The opcodes of the 125 P4-17 packets against ServerPacketsOpcodes.java (the values below are copied from its addPacketOpcode calls), and the
// helpers of serverpackets/detail/PacketSupport.h: the enum stand-ins against the Java enum constructor arguments, Java auto-unboxing and the
// Java HashMap iteration order of the packet members that hold a HashMap as an unordered container.

#include "SmLzTestSupport.h"

#include <cstdint>
#include <optional>
#include <string>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"
#include "aion/gameserver/network/aion/serverpackets/detail/PacketSupport.h"

namespace aion::gameserver::network::aion::serverpackets::testing {
namespace {

TEST(PacketOpcodesTest, EveryPacketOfTheChunkHasTheJavaOpcode) {
	// ServerPacketsOpcodes.java: addPacketOpcode(<opcode>, <class>.class) for SM_L* .. SM_Z*
#define JAVA_OPCODE(Packet, opcode) EXPECT_EQ(opcodeOf<Packet>, opcode) << #Packet;
	JAVA_OPCODE(SM_L2AUTH_LOGIN_CHECK, 199)
	JAVA_OPCODE(SM_LEARN_RECIPE, 241)
	JAVA_OPCODE(SM_LEAVE_GROUP_MEMBER, 247)
	JAVA_OPCODE(SM_LEGION_ADD_MEMBER, 111)
	JAVA_OPCODE(SM_LEGION_DOMINION_LOC_INFO, 303)
	JAVA_OPCODE(SM_LEGION_DOMINION_RANK, 302)
	JAVA_OPCODE(SM_LEGION_EDIT, 158)
	JAVA_OPCODE(SM_LEGION_HISTORY, 12)
	JAVA_OPCODE(SM_LEGION_INFO, 110)
	JAVA_OPCODE(SM_LEGION_LEAVE_MEMBER, 112)
	JAVA_OPCODE(SM_LEGION_MEMBERLIST, 157)
	JAVA_OPCODE(SM_LEGION_SEND_EMBLEM, 213)
	JAVA_OPCODE(SM_LEGION_SEND_EMBLEM_DATA, 214)
	JAVA_OPCODE(SM_LEGION_UPDATE_EMBLEM, 215)
	JAVA_OPCODE(SM_LEGION_UPDATE_MEMBER, 113)
	JAVA_OPCODE(SM_LEGION_UPDATE_NICKNAME, 11)
	JAVA_OPCODE(SM_LEGION_UPDATE_SELF_INTRO, 119)
	JAVA_OPCODE(SM_LEGION_UPDATE_TITLE, 114)
	JAVA_OPCODE(SM_LOGIN_QUEUE, 23)
	JAVA_OPCODE(SM_LOOKATOBJECT, 40)
	JAVA_OPCODE(SM_LOOT_ITEMLIST, 206)
	JAVA_OPCODE(SM_LOOT_STATUS, 205)
	JAVA_OPCODE(SM_MACRO_LIST, 231)
	JAVA_OPCODE(SM_MACRO_RESULT, 232)
	JAVA_OPCODE(SM_MAIL_SERVICE, 161)
	JAVA_OPCODE(SM_MANTRA_EFFECT, 208)
	JAVA_OPCODE(SM_MARK_FRIENDLIST, 279)
	JAVA_OPCODE(SM_MAY_LOGIN_INTO_GAME, 137)
	JAVA_OPCODE(SM_MEGAPHONE, 285)
	JAVA_OPCODE(SM_MESSAGE, 24)
	JAVA_OPCODE(SM_MOTION, 148)
	JAVA_OPCODE(SM_MOVE, 55)
	JAVA_OPCODE(SM_NEARBY_QUESTS, 127)
	JAVA_OPCODE(SM_NICKNAME_CHECK_RESPONSE, 233)
	JAVA_OPCODE(SM_NPC_ASSEMBLER, 10)
	JAVA_OPCODE(SM_NPC_INFO, 14)
	JAVA_OPCODE(SM_OBJECT_USE_UPDATE, 264)
	JAVA_OPCODE(SM_PACKAGE_INFO_NOTIFY, 266)
	JAVA_OPCODE(SM_PET, 101)
	JAVA_OPCODE(SM_PET_EMOTE, 187)
	JAVA_OPCODE(SM_PING_RESPONSE, 128)
	JAVA_OPCODE(SM_PLASTIC_SURGERY, 83)
	JAVA_OPCODE(SM_PLAYER_INFO, 32)
	JAVA_OPCODE(SM_PLAYER_REGION, 217)
	JAVA_OPCODE(SM_PLAYER_SEARCH, 211)
	JAVA_OPCODE(SM_PLAYER_SPAWN, 15)
	JAVA_OPCODE(SM_PLAYER_STANCE, 31)
	JAVA_OPCODE(SM_PLAYER_STATE, 68)
	JAVA_OPCODE(SM_PLAY_MOVIE, 105)
	JAVA_OPCODE(SM_PONG, 142)
	JAVA_OPCODE(SM_POSITION, 204)
	JAVA_OPCODE(SM_POSITION_SELF, 21)
	JAVA_OPCODE(SM_PRICES, 252)
	JAVA_OPCODE(SM_PRIVATE_STORE, 134)
	JAVA_OPCODE(SM_PRIVATE_STORE_NAME, 145)
	JAVA_OPCODE(SM_QUESTIONNAIRE, 191)
	JAVA_OPCODE(SM_QUESTION_WINDOW, 52)
	JAVA_OPCODE(SM_QUEST_ACTION, 124)
	JAVA_OPCODE(SM_QUEST_COMPLETED_LIST, 123)
	JAVA_OPCODE(SM_QUEST_LIST, 71)
	JAVA_OPCODE(SM_QUEST_REPEAT, 290)
	JAVA_OPCODE(SM_QUIT_RESPONSE, 98)
	JAVA_OPCODE(SM_RECALLED_BY_OTHER, 69)
	JAVA_OPCODE(SM_RECEIVE_BIDS, 259)
	JAVA_OPCODE(SM_RECIPE_COOLDOWN, 165)
	JAVA_OPCODE(SM_RECIPE_DELETE, 242)
	JAVA_OPCODE(SM_RECIPE_LIST, 207)
	JAVA_OPCODE(SM_RECONNECT_KEY, 255)
	JAVA_OPCODE(SM_RENAME, 88)
	JAVA_OPCODE(SM_REPURCHASE, 167)
	JAVA_OPCODE(SM_RESTORE_CHARACTER, 203)
	JAVA_OPCODE(SM_RESURRECT, 194)
	JAVA_OPCODE(SM_RIDE_ROBOT, 92)
	JAVA_OPCODE(SM_RIFT_ANNOUNCE, 236)
	JAVA_OPCODE(SM_SECONDARY_SHOW_DECOMPOSABLE, 286)
	JAVA_OPCODE(SM_SECURITY_TOKEN, 152)
	JAVA_OPCODE(SM_SELL_ITEM, 62)
	JAVA_OPCODE(SM_SHIELD_EFFECT, 218)
	JAVA_OPCODE(SM_SHOW_BRAND, 249)
	JAVA_OPCODE(SM_SHOW_NPC_ON_MAP, 89)
	JAVA_OPCODE(SM_SIEGE_LOCATION_INFO, 209)
	JAVA_OPCODE(SM_SIEGE_LOCATION_STATE, 210)
	JAVA_OPCODE(SM_SKILL_ACTIVATION, 46)
	JAVA_OPCODE(SM_SKILL_CANCEL, 42)
	JAVA_OPCODE(SM_SKILL_COOLDOWN, 51)
	JAVA_OPCODE(SM_SKILL_LIST, 44)
	JAVA_OPCODE(SM_SKILL_REMOVE, 45)
	JAVA_OPCODE(SM_STATS_INFO, 1)
	JAVA_OPCODE(SM_STATS_STATUS_UNK, 276)
	JAVA_OPCODE(SM_STATUPDATE_DP, 6)
	JAVA_OPCODE(SM_STATUPDATE_EXP, 8)
	JAVA_OPCODE(SM_STATUPDATE_HP, 3)
	JAVA_OPCODE(SM_STATUPDATE_MP, 4)
	JAVA_OPCODE(SM_SUMMON_OWNER_REMOVE, 154)
	JAVA_OPCODE(SM_SUMMON_PANEL, 153)
	JAVA_OPCODE(SM_SUMMON_PANEL_REMOVE, 73)
	JAVA_OPCODE(SM_SUMMON_UPDATE, 155)
	JAVA_OPCODE(SM_SUMMON_USESKILL, 162)
	JAVA_OPCODE(SM_TARGET_SELECTED, 41)
	JAVA_OPCODE(SM_TARGET_UPDATE, 81)
	JAVA_OPCODE(SM_TELEPORT_LOC, 20)
	JAVA_OPCODE(SM_TELEPORT_MAP, 196)
	JAVA_OPCODE(SM_TIME_CHECK, 39)
	JAVA_OPCODE(SM_TITLE_INFO, 176)
	JAVA_OPCODE(SM_TOWNS_LIST, 226)
	JAVA_OPCODE(SM_TRADELIST, 253)
	JAVA_OPCODE(SM_TRADE_IN_LIST, 151)
	JAVA_OPCODE(SM_TRANSFORM, 58)
	JAVA_OPCODE(SM_TRANSFORM_IN_SUMMON, 156)
	JAVA_OPCODE(SM_TUNE_RESULT, 288)
	JAVA_OPCODE(SM_UI_SETTINGS, 30)
	JAVA_OPCODE(SM_UNK_3_5_1, 150)
	JAVA_OPCODE(SM_UNWRAP_ITEM, 289)
	JAVA_OPCODE(SM_UPDATE_NOTE, 104)
	JAVA_OPCODE(SM_UPDATE_PLAYER_APPEARANCE, 36)
	JAVA_OPCODE(SM_UPGRADE_ARCADE, 298)
	JAVA_OPCODE(SM_USE_OBJECT, 197)
	JAVA_OPCODE(SM_VERSION_CHECK, 0)
	JAVA_OPCODE(SM_VIEW_PLAYER_DETAILS, 65)
	JAVA_OPCODE(SM_WAREHOUSE_ADD_ITEM, 169)
	JAVA_OPCODE(SM_WAREHOUSE_INFO, 168)
	JAVA_OPCODE(SM_WAREHOUSE_UPDATE_ITEM, 171)
	JAVA_OPCODE(SM_WEATHER, 67)
	JAVA_OPCODE(SM_WINDSTREAM, 163)
	JAVA_OPCODE(SM_WINDSTREAM_ANNOUNCE, 164)
#undef JAVA_OPCODE
}

TEST(PacketSupportTest, EnumStandInsEqualTheJavaConstructorArguments) {
	using namespace detail;
	// LegionEmblemType.java: DEFAULT(0x00), CUSTOM(0x80) as (byte)
	EXPECT_EQ(legionEmblemTypeValue(model::team::legion::LegionEmblemType::DEFAULT), 0);
	EXPECT_EQ(legionEmblemTypeValue(model::team::legion::LegionEmblemType::CUSTOM), -128);
	// LegionRank.java: BRIGADE_GENERAL(0), DEPUTY(1), CENTURION(2), LEGIONARY(3), VOLUNTEER(4)
	EXPECT_EQ(legionRankId(model::team::legion::LegionRank::BRIGADE_GENERAL), 0);
	EXPECT_EQ(legionRankId(model::team::legion::LegionRank::LEGIONARY), 3);
	EXPECT_EQ(legionRankId(model::team::legion::LegionRank::VOLUNTEER), 4);
	// LegionHistoryAction.java: CREATE(0) .. EMBLEM_MODIFIED(6), DEFENSE(11), OCCUPATION(12), LEGION_RENAME(13) .. KINAH_WITHDRAW(18)
	using model::team::legion::LegionHistoryAction;
	const std::vector<std::pair<LegionHistoryAction, int32_t>> history{{LegionHistoryAction::CREATE, 0}, {LegionHistoryAction::JOIN, 1},
		{LegionHistoryAction::KICK, 2}, {LegionHistoryAction::LEVEL_UP, 3}, {LegionHistoryAction::APPOINTED, 4},
		{LegionHistoryAction::EMBLEM_REGISTER, 5}, {LegionHistoryAction::EMBLEM_MODIFIED, 6}, {LegionHistoryAction::DEFENSE, 11},
		{LegionHistoryAction::OCCUPATION, 12}, {LegionHistoryAction::LEGION_RENAME, 13}, {LegionHistoryAction::CHARACTER_RENAME, 14},
		{LegionHistoryAction::ITEM_DEPOSIT, 15}, {LegionHistoryAction::ITEM_WITHDRAW, 16}, {LegionHistoryAction::KINAH_DEPOSIT, 17},
		{LegionHistoryAction::KINAH_WITHDRAW, 18}};
	ASSERT_EQ(history.size(), xml::EnumTraits<LegionHistoryAction>::names.size());
	for (const auto& [action, id] : history)
		EXPECT_EQ(legionHistoryActionId(action), id) << xml::EnumTraits<LegionHistoryAction>::names[static_cast<size_t>(action)];
	// HouseDoorState.java: OPEN(1), CLOSED_EXCEPT_FRIENDS(2), CLOSED(3)
	EXPECT_EQ(houseDoorStateId(model::house::HouseDoorState::OPEN), 1);
	EXPECT_EQ(houseDoorStateId(model::house::HouseDoorState::CLOSED_EXCEPT_FRIENDS), 2);
	EXPECT_EQ(houseDoorStateId(model::house::HouseDoorState::CLOSED), 3);
	// SiegeRace.java: ELYOS(Race.ELYOS = 0), ASMODIANS(Race.ASMODIANS = 1), BALAUR(2, 900242)
	EXPECT_EQ(siegeRaceId(model::siege::SiegeRace::ELYOS), 0);
	EXPECT_EQ(siegeRaceId(model::siege::SiegeRace::ASMODIANS), 1);
	EXPECT_EQ(siegeRaceId(model::siege::SiegeRace::BALAUR), 2);
	// QuestStatus.java: START(3), REWARD(4), COMPLETE(5), LOCKED(6)
	EXPECT_EQ(questStatusValue(questEngine::model::QuestStatus::START), 3);
	EXPECT_EQ(questStatusValue(questEngine::model::QuestStatus::LOCKED), 6);
	// TransformType.java: NONE(0), PC(1), AVATAR(2), FORM1(3)
	EXPECT_EQ(transformTypeId(skillengine::model::TransformType::AVATAR), 2);
	EXPECT_EQ(transformTypeId(skillengine::model::TransformType::FORM1), 3);
	// SummonMode.java: ATTACK(0), GUARD(1), REST(2), RELEASE(3), UNK(5)
	EXPECT_EQ(summonModeId(model::summons::SummonMode::ATTACK), 0);
	EXPECT_EQ(summonModeId(model::summons::SummonMode::RELEASE), 3);
	EXPECT_EQ(summonModeId(model::summons::SummonMode::UNK), 5);
	// AbyssRankEnum.java: GRADE9_SOLDIER(1, ...) .. SUPREME_COMMANDER(18, ...)
	EXPECT_EQ(abyssRankId(utils::stats::AbyssRankEnum::GRADE9_SOLDIER), 1);
	EXPECT_EQ(abyssRankId(utils::stats::AbyssRankEnum::STAR1_OFFICER), 10);
	EXPECT_EQ(abyssRankId(utils::stats::AbyssRankEnum::SUPREME_COMMANDER), 18);
	// ItemPacketService.ItemAddType.java
	using services::item::ItemPacketService_ItemAddType;
	const std::vector<int32_t> addMasks{0x00, 0x07, 0x13, 0x19, 0x1C, 0x21, 0x23, 0x2B, 0x2D, 0x2E, 0x2F, 0x30, 0x35, 0x36, 0x36, 0x40, 0x50, 0x51};
	ASSERT_EQ(addMasks.size(), xml::EnumTraits<ItemPacketService_ItemAddType>::names.size());
	EXPECT_EQ(itemAddTypeMask(ItemPacketService_ItemAddType::ALL_SLOT), 0x13);
	EXPECT_EQ(itemAddTypeMask(ItemPacketService_ItemAddType::MAIL), 0x36);
	for (size_t i = 0; i < addMasks.size(); i++)
		EXPECT_EQ(itemAddTypeMask(static_cast<ItemPacketService_ItemAddType>(i)), addMasks[i]) << xml::EnumTraits<ItemPacketService_ItemAddType>::names[i];
	// ItemPacketService.ItemUpdateType.java: (mask, sendable)
	using services::item::ItemPacketService_ItemUpdateType;
	ASSERT_EQ(xml::EnumTraits<ItemPacketService_ItemUpdateType>::names.size(), 26u);
	EXPECT_EQ(itemUpdateTypeData(ItemPacketService_ItemUpdateType::EQUIP_UNEQUIP).mask, -1);
	EXPECT_FALSE(itemUpdateTypeData(ItemPacketService_ItemUpdateType::POLISH_CHARGE).sendable);
	EXPECT_TRUE(itemUpdateTypeData(ItemPacketService_ItemUpdateType::STATS_CHANGE).sendable);
	EXPECT_EQ(itemUpdateTypeData(ItemPacketService_ItemUpdateType::DEC_KINAH_FLY).mask, 0x4B);
	EXPECT_EQ(itemUpdateTypeData(ItemPacketService_ItemUpdateType::INC_PASSPORT_ADD).mask, 0x8A);
}

// The stand-ins shared with the P4-16 packets (SM_ATTACK, SM_CASTSPELL_RESULT, SM_GROUP_INFO, SM_ALLIANCE_INFO, SM_DELETE_ITEM,
// SM_DELETE_WAREHOUSE_ITEM, SM_ABNORMAL_EFFECT, SM_ABNORMAL_STATE, SM_GROUP_MEMBER_INFO, SM_ALLIANCE_MEMBER_INFO)
TEST(PacketSupportTest, SharedEnumStandInsEqualTheJavaConstructorArguments) {
	using namespace detail;
	// ItemPacketService.ItemDeleteType.java: DEFAULT(0), SPLIT(0x04), MOVE(0x14), DISCARD(0x15), USE(0x17), SELL(0x1F), QUEST_COMPLETE(0x31),
	// QUEST_START(0x34), DECOMPOSE(0x66), REGISTER(0x78), PUT_TO_EXCHANGE(0x26)
	using services::item::ItemPacketService_ItemDeleteType;
	const std::vector<std::pair<ItemPacketService_ItemDeleteType, int32_t>> deleteMasks{{ItemPacketService_ItemDeleteType::DEFAULT, 0},
		{ItemPacketService_ItemDeleteType::SPLIT, 0x04}, {ItemPacketService_ItemDeleteType::MOVE, 0x14},
		{ItemPacketService_ItemDeleteType::DISCARD, 0x15}, {ItemPacketService_ItemDeleteType::USE, 0x17}, {ItemPacketService_ItemDeleteType::SELL, 0x1F},
		{ItemPacketService_ItemDeleteType::QUEST_COMPLETE, 0x31}, {ItemPacketService_ItemDeleteType::QUEST_START, 0x34},
		{ItemPacketService_ItemDeleteType::DECOMPOSE, 0x66}, {ItemPacketService_ItemDeleteType::REGISTER, 0x78},
		{ItemPacketService_ItemDeleteType::PUT_TO_EXCHANGE, 0x26}};
	ASSERT_EQ(deleteMasks.size(), xml::EnumTraits<ItemPacketService_ItemDeleteType>::names.size());
	for (const auto& [type, mask] : deleteMasks)
		EXPECT_EQ(itemDeleteTypeMask(type), mask) << xml::EnumTraits<ItemPacketService_ItemDeleteType>::names[static_cast<size_t>(type)];
	// AttackStatus.java: (id, counterSkill); the one-argument constructor passes counterSkill false
	using controllers::attack::AttackStatus;
	struct AttackStatusData {
		AttackStatus status;
		int32_t id;
		bool counterSkill;
	};
	const std::vector<AttackStatusData> attackStatuses{{AttackStatus::DODGE, 0, true}, {AttackStatus::OFFHAND_DODGE, 1, true},
		{AttackStatus::PARRY, 2, true}, {AttackStatus::OFFHAND_PARRY, 3, true}, {AttackStatus::BLOCK, 4, true}, {AttackStatus::OFFHAND_BLOCK, 5, true},
		{AttackStatus::RESIST, 6, true}, {AttackStatus::OFFHAND_RESIST, 7, true}, {AttackStatus::BUF, 8, false}, {AttackStatus::OFFHAND_BUF, 9, false},
		{AttackStatus::NORMALHIT, 10, false}, {AttackStatus::OFFHAND_NORMALHIT, 11, false}, {AttackStatus::CRITICAL_DODGE, -64, true},
		{AttackStatus::CRITICAL_PARRY, -62, true}, {AttackStatus::CRITICAL_BLOCK, -60, true}, {AttackStatus::CRITICAL_RESIST, -58, true},
		{AttackStatus::CRITICAL, -54, false}, {AttackStatus::OFFHAND_CRITICAL_DODGE, -47, true}, {AttackStatus::OFFHAND_CRITICAL_PARRY, -45, true},
		{AttackStatus::OFFHAND_CRITICAL_BLOCK, -43, true}, {AttackStatus::OFFHAND_CRITICAL_RESIST, -41, true},
		{AttackStatus::OFFHAND_CRITICAL, -37, false}};
	ASSERT_EQ(attackStatuses.size(), xml::EnumTraits<AttackStatus>::names.size());
	for (const AttackStatusData& data : attackStatuses) {
		EXPECT_EQ(attackStatusId(data.status), data.id) << xml::EnumTraits<AttackStatus>::names[static_cast<size_t>(data.status)];
		EXPECT_EQ(attackStatusIsCounterSkill(data.status), data.counterSkill) << xml::EnumTraits<AttackStatus>::names[static_cast<size_t>(data.status)];
	}
	// TeamType.java: GROUP(0x3F, 0), AUTO_GROUP(0x02, 1), ALLIANCE(0x3F, 0), AUTO_ALLIANCE(0x36, 1), ALLIANCE_DEFENCE(0x3F, 4),
	// ALLIANCE_OFFENCE(0x02, 3)
	using model::team::TeamType;
	const std::vector<std::tuple<TeamType, int32_t, int32_t>> teamTypes{{TeamType::GROUP, 0x3F, 0}, {TeamType::AUTO_GROUP, 0x02, 1},
		{TeamType::ALLIANCE, 0x3F, 0}, {TeamType::AUTO_ALLIANCE, 0x36, 1}, {TeamType::ALLIANCE_DEFENCE, 0x3F, 4}, {TeamType::ALLIANCE_OFFENCE, 0x02, 3}};
	ASSERT_EQ(teamTypes.size(), xml::EnumTraits<TeamType>::names.size());
	for (const auto& [type, typeValue, subType] : teamTypes) {
		EXPECT_EQ(teamTypeType(type), typeValue) << xml::EnumTraits<TeamType>::names[static_cast<size_t>(type)];
		EXPECT_EQ(teamTypeSubType(type), subType) << xml::EnumTraits<TeamType>::names[static_cast<size_t>(type)];
	}
	// LootRuleType.java: FREEFORALL(0), ROUNDROBIN(1), LEADER(2)
	using model::team::common::legacy::LootRuleType;
	ASSERT_EQ(xml::EnumTraits<LootRuleType>::names.size(), 3u);
	EXPECT_EQ(lootRuleId(LootRuleType::FREEFORALL), 0);
	EXPECT_EQ(lootRuleId(LootRuleType::ROUNDROBIN), 1);
	EXPECT_EQ(lootRuleId(LootRuleType::LEADER), 2);
	// SkillTargetSlot.java: BUFF(1), DEBUFF(2), CHANT(4), SPEC(8), SPEC2(16), BOOST(32), NOSHOW(64), NONE(128); FULLSLOTS = 127
	using skillengine::model::SkillTargetSlot;
	const std::vector<std::pair<SkillTargetSlot, int32_t>> slots{{SkillTargetSlot::BUFF, 1}, {SkillTargetSlot::DEBUFF, 2}, {SkillTargetSlot::CHANT, 4},
		{SkillTargetSlot::SPEC, 8}, {SkillTargetSlot::SPEC2, 16}, {SkillTargetSlot::BOOST, 32}, {SkillTargetSlot::NOSHOW, 64},
		{SkillTargetSlot::NONE, 128}};
	ASSERT_EQ(slots.size(), xml::EnumTraits<SkillTargetSlot>::names.size());
	for (const auto& [slot, id] : slots)
		EXPECT_EQ(skillTargetSlotId(slot), id) << xml::EnumTraits<SkillTargetSlot>::names[static_cast<size_t>(slot)];
	EXPECT_EQ(SKILL_TARGET_SLOT_FULLSLOTS, 127);
	// effect.getTargetSlot() of an effect without a target slot: NullPointerException
	EXPECT_EQ(requireTargetSlot(SkillTargetSlot::SPEC2), SkillTargetSlot::SPEC2);
	EXPECT_THROW(requireTargetSlot(std::nullopt), runtime::NullPointerException);
}

TEST(PacketSupportTest, UnboxingANullValueThrowsNullPointerException) {
	EXPECT_EQ(detail::unbox(std::optional<int32_t>(7), "x"), 7);
	EXPECT_THROW(static_cast<void>(detail::unbox(std::optional<int32_t>(), "x")), runtime::NullPointerException);
}

TEST(PacketSupportTest, JavaHashMapOrderOfIntegerKeys) {
	// HashMap.hash spreads the high bits: 65536 hashes to 65536 ^ 1 = 65537, bucket 1 of 16, after key 1 (inserted first in ascending order)
	std::vector<std::pair<int32_t, int32_t>> spread = detail::javaHashMapOrder(std::unordered_map<int32_t, int32_t>{{65536, 3}, {2, 2}, {1, 1}});
	EXPECT_EQ(spread, (std::vector<std::pair<int32_t, int32_t>>{{1, 1}, {65536, 3}, {2, 2}}));
	// 13 entries exceed 0.75 * 16: the table has 32 buckets, so 20 (bucket 20) follows 3 and 17 follows 1
	std::unordered_map<int32_t, int32_t> grown;
	for (int32_t key : {1, 17, 3, 20, 4, 5, 6, 7, 8, 9, 10, 11, 12})
		grown.emplace(key, key);
	std::vector<int32_t> keys;
	for (const auto& entry : detail::javaHashMapOrder(grown))
		keys.push_back(entry.first);
	EXPECT_EQ(keys, (std::vector<int32_t>{1, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 17, 20}));
	// negative keys: -1 spreads to 0xFFFFFFFF ^ (0xFFFFFFFF >>> 16) = 0xFFFF0000, bucket 0, before 14 (bucket 14)
	EXPECT_EQ(detail::javaHashSetOrder(std::unordered_set<int32_t>{-1, 14}), (std::vector<int32_t>{-1, 14}));
}

} // namespace
} // namespace aion::gameserver::network::aion::serverpackets::testing
