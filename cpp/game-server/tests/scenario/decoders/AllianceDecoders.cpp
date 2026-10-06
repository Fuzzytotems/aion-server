#include "decoders/AllianceDecoders.h"

#include <string>

namespace aion::gameserver::scenario::decoders {

// ---- SM_ALLIANCE_INFO --------------------------------------------------------------------------------------------------------------------

AllianceInfo decodeAllianceInfo(std::span<const uint8_t> body) {
	BodyReader reader(body, "SM_ALLIANCE_INFO");
	AllianceInfo info;
	info.groupSize = reader.H();  // writeH(alliance.groupSize())
	info.allianceId = reader.D(); // writeD(groupid)
	info.leaderId = reader.D();   // writeD(leaderid)
	info.mapId = reader.D();      // writeD(player.getWorldId())
	for (int i = 0; i < 4; i++) { // the vice captain ids, then zeros up to four
		const int32_t id = reader.D();
		if (id != 0)
			info.viceCaptains.push_back(id);
	}
	for (int i = 0; i < 8; i++) // the loot rule id, misc and the six quality words
		info.lootWords.push_back(reader.D());
	reader.expectD(0x02, "SM_ALLIANCE_INFO writeD(0x02)");
	reader.expectC(0x00, "SM_ALLIANCE_INFO writeC(0x00)");
	info.type = reader.D();
	info.subType = reader.D();
	info.leagueId = reader.D();
	for (int32_t a = 0; a < 4; a++) { // writeD(a) group num, writeD(1000 + a) group id
		reader.expectD(a, "SM_ALLIANCE_INFO group num");
		reader.expectD(1000 + a, "SM_ALLIANCE_INFO group id");
	}
	info.messageId = reader.D();
	info.message = reader.S();
	if (reader.remaining() > 0) { // the league block: writeH(leagueData.size()), the league's loot words, 0x02, then each alliance
		info.leagueAlliances = reader.H();
		for (int i = 0; i < 8; i++)
			reader.D();
		reader.expectD(0x02, "SM_ALLIANCE_INFO league writeD(0x02)");
		for (int32_t i = 0; i < info.leagueAlliances; i++) {
			reader.D(); // alliance position
			reader.D(); // alliance object id
			reader.D(); // member count
			reader.S(); // captain name
			reader.D(); // captain world id
		}
	}
	reader.expectFullyConsumed();
	return info;
}

// ---- SM_ALLIANCE_MEMBER_INFO -------------------------------------------------------------------------------------------------------------

namespace {

/** the effect list of UPDATE_EFFECTS and of the online ENTER family: writeC(slot), writeH(count), the effects, eight writeD(0x00) */
void readEffects(BodyReader& reader, AllianceMemberInfo& info) {
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
	for (int i = 0; i < 8; i++)
		reader.expectD(0, "SM_ALLIANCE_MEMBER_INFO trailing writeD(0x00)");
}

} // namespace

AllianceMemberInfo decodeAllianceMemberInfo(std::span<const uint8_t> body) {
	BodyReader reader(body, "SM_ALLIANCE_MEMBER_INFO");
	AllianceMemberInfo info;
	info.allianceGroupId = reader.D(); // writeD(allianceId)
	info.objectId = reader.D();        // writeD(objectId)
	info.maxHp = reader.D();           // the six life stats, zeros for an offline member
	info.currentHp = reader.D();
	info.maxMp = reader.D();
	info.currentMp = reader.D();
	info.maxFp = reader.D();
	info.currentFp = reader.D();
	reader.expectD(0, "SM_ALLIANCE_MEMBER_INFO writeD(0) unk 3.5");
	info.mapId = reader.D();
	info.instanceKey = reader.D(); // mapId + instanceId - 1
	info.x = reader.F();
	info.y = reader.F();
	info.z = reader.F();
	info.classId = reader.C();
	info.genderId = reader.C();
	info.level = reader.C();
	info.event = reader.C(); // event.getId()
	reader.expectC(1, "SM_ALLIANCE_MEMBER_INFO writeC(1)");
	info.flyState = reader.C();
	reader.expectC(0, "SM_ALLIANCE_MEMBER_INFO writeC(0x0)");
	switch (info.event) {
		case ALLIANCE_EVENT_LEAVE: // LEAVE, BANNED
		case ALLIANCE_EVENT_MOVEMENT:
		case ALLIANCE_EVENT_DISCONNECTED:
			break;
		case ALLIANCE_EVENT_UPDATE_EFFECTS:
			reader.expectD(0, "SM_ALLIANCE_MEMBER_INFO UPDATE_EFFECTS unk");
			reader.expectD(0, "SM_ALLIANCE_MEMBER_INFO UPDATE_EFFECTS unk");
			readEffects(reader, info);
			break;
		case ALLIANCE_EVENT_JOIN: // JOIN and MEMBER_GROUP_CHANGE share id 5: the group change ends after the name
		case ALLIANCE_EVENT_ENTER_OFFLINE:
		case ALLIANCE_EVENT_ENTER: // ENTER, UPDATE, RECONNECT and the captain events
			info.name = reader.S();
			if (info.event == ALLIANCE_EVENT_JOIN && reader.remaining() == 0) {
				info.groupChange = true;
				break;
			}
			reader.expectD(0, "SM_ALLIANCE_MEMBER_INFO unk");
			reader.expectD(0, "SM_ALLIANCE_MEMBER_INFO unk");
			if (reader.remaining() == 2)
				reader.expectH(0, "SM_ALLIANCE_MEMBER_INFO offline writeH(0)");
			else
				readEffects(reader, info);
			break;
		default:
			reader.fail("event " + std::to_string(info.event) + " is no PlayerAllianceEvent id");
	}
	reader.expectFullyConsumed();
	return info;
}

// ---- SM_ALLIANCE_READY_CHECK -------------------------------------------------------------------------------------------------------------

AllianceReadyCheck decodeAllianceReadyCheck(std::span<const uint8_t> body) {
	BodyReader reader(body, "SM_ALLIANCE_READY_CHECK");
	AllianceReadyCheck check;
	check.playerObjectId = reader.D();
	check.statusCode = reader.C();
	reader.expectFullyConsumed();
	return check;
}

} // namespace aion::gameserver::scenario::decoders
