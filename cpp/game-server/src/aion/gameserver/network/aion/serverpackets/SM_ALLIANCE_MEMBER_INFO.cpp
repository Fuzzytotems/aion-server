#include "aion/gameserver/network/aion/serverpackets/SM_ALLIANCE_MEMBER_INFO.h"

#include "aion/gameserver/model/GenderInfo.h"
#include "aion/gameserver/model/PlayerClassInfo.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"
#include "aion/gameserver/model/stats/container/PlayerLifeStats.h"
#include "aion/gameserver/model/team/common/legacy/PlayerAllianceEvent.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"
#include "aion/gameserver/network/aion/serverpackets/detail/PacketSupport.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/skillengine/model/Effect.h"
#include "aion/gameserver/skillengine/model/SkillTargetSlot.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::network::aion::serverpackets {

namespace {

/** Java: PlayerAllianceEvent.getId() in ordinal order (PlayerAllianceEvent.java constructor arguments) */
int32_t allianceEventId(model::team::common::legacy::PlayerAllianceEvent event) {
	// LEAVE, BANNED, MOVEMENT, DISCONNECTED, JOIN, ENTER_OFFLINE, UPDATE_EFFECTS, RECONNECT, ENTER, UPDATE, MEMBER_GROUP_CHANGE, APPOINT_VICE_CAPTAIN,
	// DEMOTE_VICE_CAPTAIN, APPOINT_CAPTAIN
	static constexpr int32_t IDS[] = {0, 0, 1, 3, 5, 7, 65, 13, 13, 13, 5, 13, 13, 13};
	return IDS[static_cast<size_t>(event)];
}

} // namespace

SM_ALLIANCE_MEMBER_INFO::SM_ALLIANCE_MEMBER_INFO(model::team::alliance::PlayerAllianceMember& member,
	model::team::common::legacy::PlayerAllianceEvent eventValue, int32_t slotValue)
	: AionServerPacket(opcodeOf<SM_ALLIANCE_MEMBER_INFO>) {
	// Java: member.getObject(), member.getAllianceId(), member.getObjectId() and the player's abnormal effects. PlayerAllianceMember.h (P5-10) is
	// not written yet, so the member cannot be read.
	static_cast<void>(member);
	static_cast<void>(eventValue);
	static_cast<void>(slotValue);
	AION_UNPORTED();
}

SM_ALLIANCE_MEMBER_INFO::SM_ALLIANCE_MEMBER_INFO(model::team::alliance::PlayerAllianceMember& member,
	model::team::common::legacy::PlayerAllianceEvent eventValue)
	: SM_ALLIANCE_MEMBER_INFO(member, eventValue, 0) {
}

SM_ALLIANCE_MEMBER_INFO::~SM_ALLIANCE_MEMBER_INFO() = default;

void SM_ALLIANCE_MEMBER_INFO::writeImpl(AionConnection* con) {
	using model::team::common::legacy::PlayerAllianceEvent;
	runtime::Ptr<model::gameobjects::player::PlayerCommonData> pcd = player->getCommonData();
	runtime::Ptr<world::WorldPosition> wp = player->getPosition();

	// Required so that when member is disconnected, and his playerAllianceGroup slot is changed, he will continue to appear as disconnected to the
	// alliance. C++: changes the packet's event field once per serialization (runtime-architecture.md §8.5: serialized per recipient)
	if (event == PlayerAllianceEvent::ENTER && !player->isOnline())
		event = PlayerAllianceEvent::ENTER_OFFLINE;
	writeD(allianceId);
	writeD(objectId);
	if (player->isOnline()) {
		runtime::Ptr<model::stats::container::PlayerLifeStats> pls = player->getLifeStats();
		writeD(pls->getMaxHp());
		writeD(pls->getCurrentHp());
		writeD(pls->getMaxMp());
		writeD(pls->getCurrentMp());
		writeD(pls->getMaxFp());
		writeD(pls->getCurrentFp());
	} else {
		writeD(0);
		writeD(0);
		writeD(0);
		writeD(0);
		writeD(0);
		writeD(0);
	}
	writeD(0); // unk 3.5
	writeD(wp->getMapId());
	writeD(wp->getMapId() + wp->getInstanceId() - 1);
	writeF(wp->getX());
	writeF(wp->getY());
	writeF(wp->getZ());
	writeC(model::getClassId(pcd->getPlayerClass()));
	writeC(model::getGenderId(pcd->getGender()));
	writeC(pcd->getLevel());
	writeC(allianceEventId(event));
	writeC(1); // unk, always 0x01 since removal of Sarpan & Tiamaranta
	writeC(player->getFlyState()); // isFly
	writeC(0x0);

	switch (event) {
		case PlayerAllianceEvent::LEAVE:
		case PlayerAllianceEvent::BANNED:
		case PlayerAllianceEvent::MOVEMENT:
		case PlayerAllianceEvent::DISCONNECTED:
			break;
		case PlayerAllianceEvent::UPDATE_EFFECTS:
			writeD(0x00); // unk
			writeD(0x00); // unk
			writeC(slot);
			writeH(static_cast<int32_t>(abnormalEffects.size())); // Abnormal effects
			for (const runtime::Ref<skillengine::model::Effect>& effect : abnormalEffects) {
				writeD(effect->getEffectorId()); // casterid
				writeH(effect->getSkillId()); // spellid
				writeC(effect->getSkillLevel()); // spell level
				writeC(static_cast<int32_t>(detail::requireTargetSlot(effect->getTargetSlot()))); // unk ?
				writeD(effect->getRemainingTimeToDisplay()); // estimatedtime
			}
			writeD(0x00);
			writeD(0x00);
			writeD(0x00);
			writeD(0x00);
			writeD(0x00);
			writeD(0x00);
			writeD(0x00);
			writeD(0x00);
			break;
		case PlayerAllianceEvent::JOIN:
		case PlayerAllianceEvent::ENTER:
		case PlayerAllianceEvent::ENTER_OFFLINE:
		case PlayerAllianceEvent::UPDATE:
		case PlayerAllianceEvent::RECONNECT:
		case PlayerAllianceEvent::APPOINT_VICE_CAPTAIN: // Unused maybe...
		case PlayerAllianceEvent::DEMOTE_VICE_CAPTAIN:
		case PlayerAllianceEvent::APPOINT_CAPTAIN:
			writeS(pcd->getName());
			writeD(0x00); // unk
			writeD(0x00); // unk
			if (player->isOnline()) {
				writeC(detail::SKILL_TARGET_SLOT_FULLSLOTS);
				writeH(static_cast<int32_t>(abnormalEffects.size())); // Abnormal effects
				for (const runtime::Ref<skillengine::model::Effect>& effect : abnormalEffects) {
					writeD(effect->getEffectorId()); // casterid
					writeH(effect->getSkillId()); // spellid
					writeC(effect->getSkillLevel()); // spell level
					writeC(static_cast<int32_t>(detail::requireTargetSlot(effect->getTargetSlot()))); // unk ?
					writeD(effect->getRemainingTimeToDisplay()); // estimatedtime
				}
				writeD(0x00);
				writeD(0x00);
				writeD(0x00);
				writeD(0x00);
				writeD(0x00);
				writeD(0x00);
				writeD(0x00);
				writeD(0x00);
			} else {
				writeH(0);
			}
			break;
		case PlayerAllianceEvent::MEMBER_GROUP_CHANGE:
			writeS(pcd->getName());
			break;
	}
}

} // namespace aion::gameserver::network::aion::serverpackets
