#pragma once

// The party packets the M5g gate compares the server against (m5g-plan.md H-02, §10.3): SM_GROUP_INFO, SM_GROUP_MEMBER_INFO,
// SM_LEAVE_GROUP_MEMBER, SM_SHOW_BRAND, SM_GROUP_LOOT, SM_GROUP_DATA_EXCHANGE, SM_ABYSS_RANK_UPDATE, SM_FIND_GROUP (the recruitment list and its
// removal) and SM_RECALLED_BY_OTHER.
//
// **m5a-plan.md D9:** every layout below is written from the Java `writeImpl` under
// game-server/src/com/aionemu/gameserver/network/aion/serverpackets/ and the model classes it calls (TeamType.java, GroupEvent.java,
// SkillTargetSlot.java, LootGroupRules.java). Nothing here includes or mirrors a C++ serverpackets header. Every decode function consumes the
// body exactly and throws DecodeError otherwise; Java's literal constants are verified, not skipped.

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "decoders/PacketDecoders.h" // BodyReader and DecodeError

namespace aion::gameserver::scenario::decoders {

/** GroupEvent.getId() (GroupEvent.java:8-15) */
constexpr uint8_t GROUP_EVENT_LEAVE = 0;
constexpr uint8_t GROUP_EVENT_MOVEMENT = 1;
constexpr uint8_t GROUP_EVENT_DISCONNECTED = 3;
constexpr uint8_t GROUP_EVENT_JOIN = 5;
constexpr uint8_t GROUP_EVENT_ENTER_OFFLINE = 7;
constexpr uint8_t GROUP_EVENT_ENTER = 13; // also UPDATE
constexpr uint8_t GROUP_EVENT_UPDATE_EFFECTS = 65;

/** SkillTargetSlot's eight constants (SkillTargetSlot.java:12-19): the trailing D per value of the ENTER / UPDATE_EFFECTS blocks */
constexpr size_t SKILL_TARGET_SLOT_COUNT = 8;
/** SkillTargetSlot.FULLSLOTS (BUFF | DEBUFF | CHANT | SPEC | SPEC2 | BOOST | NOSHOW), the slot byte of ENTER / UPDATE */
constexpr uint8_t SKILL_TARGET_SLOT_FULLSLOTS = 127;

/** SM_GROUP_INFO.java:28-48 */
struct GroupInfo {
	int32_t groupId = 0;
	int32_t leaderId = 0;
	int32_t mapId = 0;
	/** lootRule id, misc, common, superior, heroic, fabled, eternal, mythic (LootGroupRules) */
	std::vector<int32_t> lootWords;
	int32_t type = 0;
	int32_t subType = 0;
	int32_t messageId = 0;
	std::string name;
};
GroupInfo decodeGroupInfo(std::span<const uint8_t> body);

/** one effect entry of SM_GROUP_MEMBER_INFO's ENTER / UPDATE_EFFECTS block */
struct GroupMemberEffect {
	int32_t effectorId = 0;
	uint16_t skillId = 0;
	uint8_t skillLevel = 0;
	uint8_t targetSlotOrdinal = 0;
	int32_t remainingMillis = 0;
};

/** SM_GROUP_MEMBER_INFO.java:48-143 */
struct GroupMemberInfo {
	int32_t groupId = 0;
	int32_t objectId = 0;
	int32_t maxHp = 0, currentHp = 0, maxMp = 0, currentMp = 0, maxFp = 0, currentFp = 0;
	int32_t mapId = 0;
	int32_t instanceKey = 0;
	float x = 0, y = 0, z = 0;
	uint8_t classId = 0, genderId = 0, level = 0;
	uint8_t event = 0;
	uint8_t flyState = 0;
	bool mentor = false;
	std::optional<std::string> name;
	/** the slot byte of UPDATE_EFFECTS (the requested slot) or ENTER / UPDATE (FULLSLOTS) */
	std::optional<uint8_t> slot;
	std::vector<GroupMemberEffect> effects;
};
GroupMemberInfo decodeGroupMemberInfo(std::span<const uint8_t> body);

/** SM_LEAVE_GROUP_MEMBER.java:12-19: five constants; the decoder verifies them all */
void decodeLeaveGroupMember(std::span<const uint8_t> body);

/** SM_SHOW_BRAND.java:30-37: brand id -> target object id */
struct ShowBrand {
	std::vector<std::pair<int32_t, int32_t>> brands;
};
ShowBrand decodeShowBrand(std::span<const uint8_t> body);

/** SM_GROUP_LOOT.java:41-55 */
struct GroupLoot {
	int32_t groupId = 0;
	int32_t index = 0;
	int32_t itemCount = 0;
	int32_t itemId = 0;
	int32_t lootCorpseId = 0;
	uint8_t distributionId = 0;
	int32_t playerId = 0;
	int32_t luck = 0;
};
GroupLoot decodeGroupLoot(std::span<const uint8_t> body);

/** SM_GROUP_DATA_EXCHANGE.java:28-36 */
struct GroupDataExchange {
	uint8_t action = 0;
	std::optional<uint8_t> unk2;
	std::vector<uint8_t> data;
};
GroupDataExchange decodeGroupDataExchange(std::span<const uint8_t> body);

/** SM_ABYSS_RANK_UPDATE.java:25-41 */
struct AbyssRankUpdate {
	uint8_t action = 0;
	int32_t objectId = 0;
	int32_t value = 0;
};
AbyssRankUpdate decodeAbyssRankUpdate(std::span<const uint8_t> body);

/** one recruitment of SM_FIND_GROUP action 0 (SM_FIND_GROUP.java showRecruitments) */
struct FindGroupRecruitment {
	int32_t objectId = 0;
	uint8_t serverId = 0;
	uint8_t soloFlag = 0; // 16: solo, 0: group or alliance
	uint8_t groupType = 0;
	std::string message;
	std::string name;
	uint8_t size = 0, minLevel = 0, maxLevel = 0;
	int32_t lastUpdate = 0;
};

/** SM_FIND_GROUP: action 0 (the recruitment list) and action 1 (a recruitment's removal); every other action keeps only its action byte */
struct FindGroup {
	uint8_t action = 0;
	std::vector<FindGroupRecruitment> recruitments;
	int32_t removedId = 0;
	uint8_t removedSoloFlag = 0;
};
FindGroup decodeFindGroup(std::span<const uint8_t> body);

/** SM_RECALLED_BY_OTHER.java: the question (open) or its close */
struct RecalledByOther {
	bool open = false;
	std::string casterName;
	int32_t skillId = 0;
	int32_t seconds = 0;
};
RecalledByOther decodeRecalledByOther(std::span<const uint8_t> body);

} // namespace aion::gameserver::scenario::decoders
