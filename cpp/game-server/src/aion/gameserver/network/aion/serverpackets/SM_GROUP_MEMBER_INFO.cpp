#include "aion/gameserver/network/aion/serverpackets/SM_GROUP_MEMBER_INFO.h"

#include "aion/gameserver/controllers/effect/PlayerEffectController.h"
#include "aion/gameserver/model/GenderInfo.h"
#include "aion/gameserver/model/PlayerClassInfo.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"
#include "aion/gameserver/model/stats/container/PlayerLifeStats.h"
#include "aion/gameserver/model/team/GeneralTeam.h"
#include "aion/gameserver/model/team/common/legacy/GroupEvent.h"
#include "aion/gameserver/model/team/group/PlayerGroup.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"
#include "aion/gameserver/network/aion/serverpackets/detail/PacketSupport.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/skillengine/model/Effect.h"
#include "aion/gameserver/skillengine/model/SkillTargetSlot.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::network::aion::serverpackets {

namespace {

/** Java: SkillTargetSlot.values().length */
constexpr int32_t SKILL_TARGET_SLOT_COUNT = 8;

/** Java: GroupEvent.getId() in ordinal order - LEAVE(0), MOVEMENT(1), DISCONNECTED(3), JOIN(5), ENTER_OFFLINE(7), ENTER(13), UPDATE(13), UPDATE_EFFECTS(65) */
int32_t groupEventId(model::team::common::legacy::GroupEvent event) {
	static constexpr int32_t IDS[] = {0, 1, 3, 5, 7, 13, 13, 65};
	return IDS[static_cast<size_t>(event)];
}

} // namespace

SM_GROUP_MEMBER_INFO::SM_GROUP_MEMBER_INFO(model::team::group::PlayerGroup& group, model::gameobjects::player::Player& playerValue,
	model::team::common::legacy::GroupEvent eventValue, int32_t slotValue)
	: AionServerPacket(opcodeOf<SM_GROUP_MEMBER_INFO>), groupId(group.getTeamId()), player(playerValue), event(eventValue), slot(slotValue) {
	using model::team::common::legacy::GroupEvent;
	switch (eventValue) {
		case GroupEvent::ENTER:
		case GroupEvent::UPDATE:
			for (runtime::Ptr<skillengine::model::Effect> effect : playerValue.getEffectController()->getAbnormalEffectsToShow())
				abnormalEffects.emplace_back(effect);
			break;
		case GroupEvent::UPDATE_EFFECTS:
			for (runtime::Ptr<skillengine::model::Effect> effect : playerValue.getEffectController()->getAbnormalEffectsToTargetSlot(slotValue))
				abnormalEffects.emplace_back(effect);
			break;
		default:
			break;
	}
}

SM_GROUP_MEMBER_INFO::SM_GROUP_MEMBER_INFO(model::team::group::PlayerGroup& group, model::gameobjects::player::Player& playerValue,
	model::team::common::legacy::GroupEvent eventValue)
	: SM_GROUP_MEMBER_INFO(group, playerValue, eventValue, 0) {
}

SM_GROUP_MEMBER_INFO::~SM_GROUP_MEMBER_INFO() = default;

void SM_GROUP_MEMBER_INFO::writeImpl(AionConnection* con) {
	using model::team::common::legacy::GroupEvent;
	runtime::Ptr<model::stats::container::PlayerLifeStats> pls = player->getLifeStats();
	runtime::Ptr<model::gameobjects::player::PlayerCommonData> pcd = player->getCommonData();
	runtime::Ptr<world::WorldPosition> wp = player->getPosition();
	// C++: Java changes the packet's event field here, once per serialization (runtime-architecture.md §8.5: serialized per recipient)
	if (event == GroupEvent::ENTER && !player->isOnline()) {
		event = GroupEvent::ENTER_OFFLINE;
	}
	writeD(groupId);
	writeD(player->getObjectId());
	if (player->isOnline()) {
		writeD(pls->getMaxHp());
		writeD(pls->getCurrentHp());
		writeD(pls->getMaxMp());
		writeD(pls->getCurrentMp());
		writeD(pls->getMaxFp()); // maxflighttime
		writeD(pls->getCurrentFp()); // currentflighttime
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
	writeC(model::getClassId(pcd->getPlayerClass())); // class id
	writeC(model::getGenderId(pcd->getGender())); // gender id
	writeC(pcd->getLevel()); // level
	writeC(groupEventId(event)); // something events
	writeC(1); // unk, always 0x01 since removal of Sarpan & Tiamarana
	writeC(player->getFlyState()); // isFly
	writeC(player->isMentor() ? 0x01 : 0x00);
	switch (event) {
		case GroupEvent::MOVEMENT:
		case GroupEvent::DISCONNECTED:
		case GroupEvent::LEAVE:
			break;
		case GroupEvent::ENTER_OFFLINE:
		case GroupEvent::JOIN:
			writeS(pcd->getName()); // name
			break;
		case GroupEvent::UPDATE_EFFECTS:
			writeD(0x00); // unk
			writeD(0x00); // unk
			writeC(slot);
			writeH(static_cast<int32_t>(abnormalEffects.size())); // Abnormal effects of slot type
			for (const runtime::Ref<skillengine::model::Effect>& effect : abnormalEffects) {
				writeD(effect->getEffectorId()); // casterid
				writeH(effect->getSkillId()); // spellid
				writeC(effect->getSkillLevel()); // spell level
				writeC(static_cast<int32_t>(detail::requireTargetSlot(effect->getTargetSlot()))); // unk ?
				writeD(effect->getRemainingTimeToDisplay()); // estimatedtime
			}
			for (int32_t targetSlot = 0; targetSlot < SKILL_TARGET_SLOT_COUNT; targetSlot++) {
				writeD(0x00); // Java: (slot & targetSlot.getId()) == 1 ? 0 (TODO: remaining time ?) : 0
			}
			break;
		case GroupEvent::ENTER:
		case GroupEvent::UPDATE:
			writeS(pcd->getName()); // name
			writeD(0x00); // unk
			writeD(0x00); // unk
			writeC(detail::SKILL_TARGET_SLOT_FULLSLOTS);
			writeH(static_cast<int32_t>(abnormalEffects.size())); // Abnormal effects
			for (const runtime::Ref<skillengine::model::Effect>& effect : abnormalEffects) {
				writeD(effect->getEffectorId()); // casterid
				writeH(effect->getSkillId()); // spellid
				writeC(effect->getSkillLevel()); // spell level
				writeC(static_cast<int32_t>(detail::requireTargetSlot(effect->getTargetSlot()))); // unk ?
				writeD(effect->getRemainingTimeToDisplay()); // estimatedtime
			}
			for (int32_t targetSlot = 0; targetSlot < SKILL_TARGET_SLOT_COUNT; targetSlot++) {
				writeD(0x00); // Java: (FULLSLOTS & targetSlot.getId()) == 1 ? 0 (TODO: remaining time ?) : 0
			}
			break;
	}
}

} // namespace aion::gameserver::network::aion::serverpackets
