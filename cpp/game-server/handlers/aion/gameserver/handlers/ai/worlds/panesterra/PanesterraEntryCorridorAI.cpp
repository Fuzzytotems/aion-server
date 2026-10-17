#include "aion/gameserver/handlers/ai/worlds/panesterra/PanesterraEntryCorridorAI.h"

#include <string>

#include "aion/commons/utils/StringUtils.h"
#include "aion/gameserver/model/DialogAction.h"
#include "aion/gameserver/services/event/Event.h"
#include "aion/gameserver/model/siege/FortressLocation.h"
#include "aion/gameserver/model/siege/SiegeRaceInfo.h"
#include "aion/gameserver/model/templates/event/EventTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/services/SiegeService.h"
#include "aion/gameserver/services/event/EventService.h"
#include "aion/gameserver/services/panesterra/PanesterraService.h"
#include "aion/gameserver/services/panesterra/ahserion/PanesterraFactionInfo.h"

namespace aion::gameserver::handlers::ai::worlds::panesterra {

AION_AI(PanesterraEntryCorridorAI, "panesterra_entry_corridor");

/** Java: the switch of the constructor (PanesterraEntryCorridorAI.java:24-30) */
int32_t PanesterraEntryCorridorAI::relatedFortressIdOf(int32_t npcId) {
	switch (npcId) {
		case 730946:
		case 730950:
			return 10111;
		case 730947:
		case 730951:
			return 10211;
		case 730948:
		case 730952:
			return 10311;
		case 730949:
		case 730953:
			return 10411;
		default:
			return 0;
	}
}

// Java PanesterraEntryCorridorAI.java:22-31
PanesterraEntryCorridorAI::PanesterraEntryCorridorAI(Npc& owner)
	: GeneralNpcAI(owner), relatedFortressId(relatedFortressIdOf(owner.getNpcId())) {
}

// Java PanesterraEntryCorridorAI.java:33-41
bool PanesterraEntryCorridorAI::onDialogSelect(Player& player, int32_t dialogActionId, int32_t questId, int32_t extendedRewardIndex) {
	static_cast<void>(questId);
	static_cast<void>(extendedRewardIndex);
	if (dialogActionId == model::DialogAction::SETPRO1 && canTeleport(player)) {
		// teleportToFortress(player);
		PanesterraService::getInstance().teleportToEventLocation(player);
	}
	return true;
}

// Java PanesterraEntryCorridorAI.java:43-66 (getFortress(relatedFortressId).getRace() on null is Java's NullPointerException)
bool PanesterraEntryCorridorAI::canTeleport(Player& player) {
	if (player.getLevel() < 65) {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_TELEPOTER_GAB1_USER04());
		return false;
	}
	// TODO: Remove after event
	runtime::Ptr<runtime::RcHashSet<runtime::Ref<services::event::Event>>> activeEvents = services::event::EventService::getInstance().getActiveEvents();
	for (runtime::Ptr<services::event::Event> event : *activeEvents) {
		if (commons::utils::StringUtils::equalsIgnoreCase(event->getEventTemplate()->getName(), "Beyond Aion Birthday Event"))
			return true;
	}
	// Don't allow access if any fortress is under siege
	for (int32_t id : {10111, 10211, 10311, 10411}) {
		if (SiegeService::getInstance().getSiege(id) != nullptr) {
			PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_CANT_READY_PANGAEA());
			return false;
		}
	}
	runtime::Ptr<model::siege::FortressLocation> fortress = SiegeService::getInstance().getFortress(relatedFortressId);
	if (fortress == nullptr)
		throw runtime::NullPointerException("FortressLocation " + std::to_string(relatedFortressId));
	if (fortress->getRace() != model::siege::getByRace(player.getRace())) {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_CANNOT_MOVE_TO_AIRPORT_NO_ROUTE());
		return false;
	}
	return true;
}

// Java PanesterraEntryCorridorAI.java:68-71
void PanesterraEntryCorridorAI::teleportToFortress(Player& player) {
	// TODO
	player.setPanesterraFaction(services::panesterra::ahserion::getByFortressId(relatedFortressId));
}

} // namespace aion::gameserver::handlers::ai::worlds::panesterra
