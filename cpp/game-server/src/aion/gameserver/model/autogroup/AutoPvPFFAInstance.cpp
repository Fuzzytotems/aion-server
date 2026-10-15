#include "aion/gameserver/model/autogroup/AutoPvPFFAInstance.h"

#include "aion/gameserver/model/autogroup/AGPlayer.h"
#include "aion/gameserver/model/autogroup/AGQuestion.h"
#include "aion/gameserver/model/autogroup/AutoGroup.h"
#include "aion/gameserver/model/autogroup/AutoGroupTypeInfo.h"
#include "aion/gameserver/model/autogroup/LookingForParty.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/network/aion/serverpackets/SM_AUTO_GROUP.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/services/AutoGroupService.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/world/WorldMapInstance.h"

namespace aion::gameserver::model::autogroup {

using gameobjects::player::Player;

runtime::Ref<AutoPvPFFAInstance> AutoPvPFFAInstance::create(AutoGroupType agt) {
	return runtime::makeRef<AutoPvPFFAInstance>(agt);
}

AutoPvPFFAInstance::AutoPvPFFAInstance(AutoGroupType value) : AutoInstance(value) {
}

AutoPvPFFAInstance::~AutoPvPFFAInstance() = default;

// Java AutoPvPFFAInstance.java:19-24: then ((PvPArenaScore) score).setDifficultyId(agt.getDifficultId()) - PvPArenaScore is J13 (CP3)
void AutoPvPFFAInstance::onInstanceCreate(world::WorldMapInstance& value) {
	AutoInstance::onInstanceCreate(value);
	AION_UNPORTED();
}

// Java AutoPvPFFAInstance.java:26-35 (synchronized)
AGQuestion AutoPvPFFAInstance::addLookingForParty(LookingForParty& lookingForParty) {
	SYNCHRONIZED(*this) {
		if (isRegistrationDisabled(lookingForParty) || lookingForParty.getMembers().size() > 1 ||
			static_cast<int32_t>(registeredAGPlayers.size()) >= getMaxPlayers()) {
			return AGQuestion::FAILED;
		}
		registeredAGPlayers.putAll(lookingForParty.getMembers());
		return instance.get() == nullptr && static_cast<int32_t>(registeredAGPlayers.size()) == getMaxPlayers() ? AGQuestion::READY : AGQuestion::ADDED;
	}
}

// Java AutoPvPFFAInstance.java:37-56: the entry ticket (one Arena ticket, three for the Arena of Glory); without it the player leaves the
// registration (SM_AUTO_GROUP window 5) and an emptied instance is destroyed. Then ((PvPArenaScore) score).portToPosition(player) and
// instance.register: PvPArenaScore is J13 (CP3)
void AutoPvPFFAInstance::onPressEnter(Player& player) {
	if (isPvPFFAArena(agt) || isPvPSoloArena(agt) || isGloryArena(agt)) {
		int64_t size = 1;
		int32_t itemId = 186000135;
		if (isGloryArena(agt)) {
			size = 3;
			itemId = 186000185;
		}
		if (!removeItem(player, itemId, size)) {
			registeredAGPlayers.remove(player.getObjectId());
			const AutoGroup* found = getTemplate(agt);
			if (found == nullptr) // Java: agt.getTemplate().getMaskId() of a missing template
				throw runtime::NullPointerException("AutoGroupType without auto group template");
			utils::PacketSendUtility::sendPacket(player, network::aion::serverpackets::SM_AUTO_GROUP(found->getMaskId(), 5));
			if (registeredAGPlayers.isEmpty())
				services::AutoGroupService::getInstance().destroyIfPossible(*this);
			return;
		}
	}
	AION_UNPORTED();
}

// Java AutoPvPFFAInstance.java:58-61
void AutoPvPFFAInstance::onLeaveInstance(Player& player) {
	AutoInstance::unregister(player);
}

} // namespace aion::gameserver::model::autogroup
