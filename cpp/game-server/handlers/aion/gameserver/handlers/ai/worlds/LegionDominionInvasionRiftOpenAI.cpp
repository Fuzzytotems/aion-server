#include "aion/gameserver/handlers/ai/worlds/LegionDominionInvasionRiftOpenAI.h"

#include <string>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/model/DialogAction.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/model/legionDominion/LegionDominionLocation.h"
#include "aion/gameserver/model/team/legion/Legion.h"
#include "aion/gameserver/model/templates/LegionDominionInvasionRift.h"
#include "aion/gameserver/model/templates/npc/NpcTemplate.h"
#include "aion/gameserver/model/templates/npc/TalkInfo.h"
#include "aion/gameserver/network/aion/serverpackets/SM_DIALOG_WINDOW.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/services/LegionDominionService.h"
#include "aion/gameserver/services/RiftService.h"

namespace aion::gameserver::handlers::ai::worlds {

AION_AI(LegionDominionInvasionRiftOpenAI, "legion_dominion_invasion_rift_open");

// Java LegionDominionInvasionRiftOpenAI.java:32-52
void LegionDominionInvasionRiftOpenAI::handleDialogStart(Player& player) {
	const model::templates::npc::NpcTemplate* template_ = getOwner().getObjectTemplate();
	const model::templates::npc::TalkInfo* talkInfo = template_->getTalkInfo();
	if (talkInfo == nullptr || talkInfo->getSubDialogType() != model::templates::npc::SubDialogType::LEGION_DOMINION_NPC)
		return;
	if (LegionDominionService::getInstance().isInCalculationTime())
		return;
	int32_t territoryId = talkInfo->getSubDialogValue();
	runtime::Ptr<model::legionDominion::LegionDominionLocation> loc = LegionDominionService::getInstance().getLegionDominionLoc(territoryId);
	if (loc == nullptr || loc->getInvasionRift() == nullptr)
		return;
	const model::templates::LegionDominionInvasionRift* invasionRift = loc->getInvasionRift();
	int32_t dialogPageId = 10;
	if (player.getLegion() == nullptr || player.getLegion()->getOccupiedLegionDominion() != territoryId)
		dialogPageId = 1011;
	else if (player.getInventory().getFirstItemByItemId(invasionRift->getKeyItemId()) == nullptr)
		dialogPageId = 27;
	PacketSendUtility::sendPacket(player, SM_DIALOG_WINDOW(getObjectId(), dialogPageId));
}

// Java LegionDominionInvasionRiftOpenAI.java:54-75 (getTalkInfo().getSubDialogValue() on a null talk info is Java's NullPointerException)
bool LegionDominionInvasionRiftOpenAI::onDialogSelect(Player& player, int32_t dialogActionId, int32_t questId, int32_t extendedRewardIndex) {
	static_cast<void>(questId);
	static_cast<void>(extendedRewardIndex);
	if (dialogActionId == model::DialogAction::SETPRO1) {
		const model::templates::npc::TalkInfo* talkInfo = getOwner().getObjectTemplate()->getTalkInfo();
		if (talkInfo == nullptr)
			throw runtime::NullPointerException("LegionDominionInvasionRiftOpenAI: no talk_info");
		int32_t territoryId = talkInfo->getSubDialogValue();
		runtime::Ptr<model::legionDominion::LegionDominionLocation> loc = LegionDominionService::getInstance().getLegionDominionLoc(territoryId);
		if (loc != nullptr && loc->getInvasionRift() != nullptr) {
			const model::templates::LegionDominionInvasionRift* invasionRift = loc->getInvasionRift();
			if (!RiftService::getInstance().isRiftOpened(invasionRift->getRiftId())) {
				if (player.getInventory().decreaseByItemId(invasionRift->getKeyItemId(), 1)) {
					if (!LegionDominionService::getInstance().openInvasionRift(loc->getLocationId())) {
						commons::logging::LoggerFactory::getLogger("LegionDominionInvasionRiftOpenAI")
							.error(player.toString() + " tried to open rift in territory " + std::to_string(loc->getLocationId()) +
								" - rift opening failed, but item [id=" + std::to_string(invasionRift->getKeyItemId()) + "] got consumed!");
					}
				}
			}
		}
	}
	PacketSendUtility::sendPacket(player, SM_DIALOG_WINDOW(getObjectId(), 0));
	return true;
}

} // namespace aion::gameserver::handlers::ai::worlds
