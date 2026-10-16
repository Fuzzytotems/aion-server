#include "decoders/TeamDecoders.h"

#include <string>

namespace aion::gameserver::scenario::decoders {

// ---- SM_GROUP_INFO -----------------------------------------------------------------------------------------------------------------------

GroupInfo decodeGroupInfo(std::span<const uint8_t> body) {
	BodyReader reader(body, "SM_GROUP_INFO");
	GroupInfo info;
	info.groupId = reader.D();  // SM_GROUP_INFO.java:31
	info.leaderId = reader.D(); // :32
	info.mapId = reader.D();    // :33
	for (int i = 0; i < 8; i++) // :34-41: the loot rule id, misc and the six quality words
		info.lootWords.push_back(reader.D());
	reader.expectD(0x02, "SM_GROUP_INFO writeD(0x02)"); // :42
	reader.expectC(0x00, "SM_GROUP_INFO writeC(0x00)"); // :43
	info.type = reader.D();                             // :44 TeamType.getType()
	info.subType = reader.D();                          // :45 TeamType.getSubType()
	info.messageId = reader.D();                        // :46
	info.name = reader.S();                             // :47
	reader.expectFullyConsumed();
	return info;
}

// ---- SM_GROUP_MEMBER_INFO ----------------------------------------------------------------------------------------------------------------

namespace {

void readEffects(BodyReader& reader, GroupMemberInfo& info) {
	reader.expectD(0, "SM_GROUP_MEMBER_INFO writeD(0x00) unk");
	reader.expectD(0, "SM_GROUP_MEMBER_INFO writeD(0x00) unk");
	info.slot = reader.C();
	const uint16_t count = reader.H();
	for (uint16_t i = 0; i < count; i++) {
		GroupMemberEffect effect;
		effect.effectorId = reader.D();
		effect.skillId = reader.H();
		effect.skillLevel = reader.C();
		effect.targetSlotOrdinal = reader.C();
		effect.remainingMillis = reader.D();
		info.effects.push_back(effect);
	}
	for (size_t i = 0; i < SKILL_TARGET_SLOT_COUNT; i++) // one writeD(0x00) per SkillTargetSlot value
		reader.expectD(0, "SM_GROUP_MEMBER_INFO's per-slot writeD(0x00)");
}

} // namespace

GroupMemberInfo decodeGroupMemberInfo(std::span<const uint8_t> body) {
	BodyReader reader(body, "SM_GROUP_MEMBER_INFO");
	GroupMemberInfo info;
	info.groupId = reader.D();  // SM_GROUP_MEMBER_INFO.java:72
	info.objectId = reader.D(); // :73
	info.maxHp = reader.D();    // :74-89 (zeros for an offline member)
	info.currentHp = reader.D();
	info.maxMp = reader.D();
	info.currentMp = reader.D();
	info.maxFp = reader.D();
	info.currentFp = reader.D();
	reader.expectD(0, "SM_GROUP_MEMBER_INFO writeD(0) unk 3.5"); // :91
	info.mapId = reader.D();                                    // :92
	info.instanceKey = reader.D();                              // :93 mapId + instanceId - 1
	info.x = reader.F();
	info.y = reader.F();
	info.z = reader.F();
	info.classId = reader.C();  // :97
	info.genderId = reader.C(); // :98
	info.level = reader.C();    // :99
	info.event = reader.C();    // :101
	reader.expectC(1, "SM_GROUP_MEMBER_INFO writeC(1)"); // :102
	info.flyState = reader.C();                          // :103
	const uint8_t mentor = reader.C();                   // :104
	if (mentor > 1)
		reader.fail("mentor byte " + std::to_string(mentor));
	info.mentor = mentor == 1;
	switch (info.event) {
		case GROUP_EVENT_MOVEMENT:
		case GROUP_EVENT_DISCONNECTED:
		case GROUP_EVENT_LEAVE:
			break;
		case GROUP_EVENT_ENTER_OFFLINE:
		case GROUP_EVENT_JOIN:
			info.name = reader.S(); // :111-114
			break;
		case GROUP_EVENT_UPDATE_EFFECTS:
			readEffects(reader, info); // :115-132
			break;
		case GROUP_EVENT_ENTER:
			info.name = reader.S(); // :133-136
			readEffects(reader, info);
			if (info.slot != SKILL_TARGET_SLOT_FULLSLOTS)
				reader.fail("ENTER / UPDATE slot byte " + std::to_string(*info.slot) + " is not FULLSLOTS");
			break;
		default:
			reader.fail("event " + std::to_string(info.event) + " is no GroupEvent id");
	}
	reader.expectFullyConsumed();
	return info;
}

// ---- SM_LEAVE_GROUP_MEMBER ---------------------------------------------------------------------------------------------------------------

void decodeLeaveGroupMember(std::span<const uint8_t> body) {
	BodyReader reader(body, "SM_LEAVE_GROUP_MEMBER");
	reader.expectD(0x00, "writeD(0x00)"); // SM_LEAVE_GROUP_MEMBER.java:14
	reader.expectC(0x00, "writeC(0x00)"); // :15
	reader.expectD(0x3F, "writeD(0x3F)"); // :16
	reader.expectD(0x00, "writeD(0x00)"); // :17
	reader.expectH(0x00, "writeH(0x00)"); // :18
	reader.expectFullyConsumed();
}

// ---- SM_SHOW_BRAND -----------------------------------------------------------------------------------------------------------------------

ShowBrand decodeShowBrand(std::span<const uint8_t> body) {
	BodyReader reader(body, "SM_SHOW_BRAND");
	ShowBrand brand;
	const uint16_t count = reader.H(); // SM_SHOW_BRAND.java:31
	for (uint16_t i = 0; i < count; i++) {
		reader.expectD(1, "SM_SHOW_BRAND writeD(1)"); // :33
		const int32_t iconId = reader.D();            // :34
		const int32_t target = reader.D();            // :35
		brand.brands.emplace_back(iconId, target);
	}
	reader.expectFullyConsumed();
	return brand;
}

// ---- SM_GROUP_LOOT -----------------------------------------------------------------------------------------------------------------------

GroupLoot decodeGroupLoot(std::span<const uint8_t> body) {
	BodyReader reader(body, "SM_GROUP_LOOT");
	GroupLoot loot;
	loot.groupId = reader.D();   // SM_GROUP_LOOT.java:42
	loot.index = reader.D();     // :43
	loot.itemCount = reader.D(); // :44
	loot.itemId = reader.D();    // :45
	reader.expectC(0, "SM_GROUP_LOOT writeC(unk3 = 0)"); // :46
	reader.expectC(0, "SM_GROUP_LOOT writeC(0) 3.0");    // :47
	reader.expectC(0, "SM_GROUP_LOOT writeC(0) 3.5");    // :48
	loot.lootCorpseId = reader.D();                      // :49
	loot.distributionId = reader.C();                    // :50
	loot.playerId = reader.D();                          // :51
	loot.luck = reader.D();                              // :52
	reader.expectFullyConsumed();
	return loot;
}

// ---- SM_GROUP_DATA_EXCHANGE --------------------------------------------------------------------------------------------------------------

GroupDataExchange decodeGroupDataExchange(std::span<const uint8_t> body) {
	BodyReader reader(body, "SM_GROUP_DATA_EXCHANGE");
	GroupDataExchange exchange;
	exchange.action = reader.C(); // SM_GROUP_DATA_EXCHANGE.java:30
	if (exchange.action != 1)
		exchange.unk2 = reader.C(); // :32-33
	const int32_t size = reader.D(); // :35
	if (size < 0)
		reader.fail("negative data length");
	exchange.data = reader.B(static_cast<size_t>(size)); // :36
	reader.expectFullyConsumed();
	return exchange;
}

// ---- SM_ABYSS_RANK_UPDATE ----------------------------------------------------------------------------------------------------------------

AbyssRankUpdate decodeAbyssRankUpdate(std::span<const uint8_t> body) {
	BodyReader reader(body, "SM_ABYSS_RANK_UPDATE");
	AbyssRankUpdate update;
	update.action = reader.C();   // SM_ABYSS_RANK_UPDATE.java:26
	update.objectId = reader.D(); // :27
	if (update.action <= 2)       // :28-40: every known action writes one D
		update.value = reader.D();
	reader.expectFullyConsumed();
	return update;
}

// ---- SM_FIND_GROUP -----------------------------------------------------------------------------------------------------------------------

FindGroup decodeFindGroup(std::span<const uint8_t> body) {
	BodyReader reader(body, "SM_FIND_GROUP");
	FindGroup result;
	result.action = reader.C(); // SM_FIND_GROUP writeImpl
	if (result.action == 0) {
		const uint16_t count = reader.H(); // showRecruitments: size twice, then the time
		reader.expectH(count, "SM_FIND_GROUP the second size");
		reader.D();
		for (uint16_t i = 0; i < count; i++) {
			FindGroupRecruitment r;
			r.objectId = reader.D();
			r.serverId = reader.C();
			reader.expectC(0, "SM_FIND_GROUP writeC(0) unk");
			reader.expectC(0, "SM_FIND_GROUP writeC(0) unk");
			r.soloFlag = reader.C();
			r.groupType = reader.C();
			r.message = reader.S();
			r.name = reader.S();
			r.size = reader.C();
			r.minLevel = reader.C();
			r.maxLevel = reader.C();
			r.lastUpdate = reader.D();
			result.recruitments.push_back(r);
		}
		reader.expectFullyConsumed();
	} else if (result.action == 1) {
		result.removedId = reader.D(); // removeRecruitment
		reader.C();                    // serverId
		reader.expectC(0, "SM_FIND_GROUP writeC(unk1)");
		reader.expectC(0, "SM_FIND_GROUP writeC(unk2)");
		result.removedSoloFlag = reader.C();
		reader.expectFullyConsumed();
	}
	return result;
}

// ---- SM_RECALLED_BY_OTHER ----------------------------------------------------------------------------------------------------------------

RecalledByOther decodeRecalledByOther(std::span<const uint8_t> body) {
	BodyReader reader(body, "SM_RECALLED_BY_OTHER");
	RecalledByOther recall;
	const uint8_t close = reader.C(); // 0 = open the window, 1 = close it
	if (close > 1)
		reader.fail("open/close byte " + std::to_string(close));
	recall.open = close == 0;
	recall.casterName = reader.S();
	recall.skillId = reader.H();
	recall.seconds = reader.H();
	reader.expectFullyConsumed();
	return recall;
}

} // namespace aion::gameserver::scenario::decoders
