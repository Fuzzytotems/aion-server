#include "aion/gameserver/handlers/ai/portals/PortalRequestAI.h"

#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/TeleLocationData.h"
#include "aion/gameserver/model/animations/TeleportAnimation.h"
#include "aion/gameserver/model/gameobjects/player/RequestResponseHandler.h"
#include "aion/gameserver/model/gameobjects/player/ResponseRequester.h"
#include "aion/gameserver/model/templates/npc/NpcTemplate.h"
#include "aion/gameserver/model/templates/teleport/TeleLocIdData.h"
#include "aion/gameserver/model/templates/teleport/TelelocationTemplate.h"
#include "aion/gameserver/model/templates/teleport/TeleporterTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_QUESTION_WINDOW.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/services/teleport/TeleportService.h"
#include "aion/gameserver/services/trade/PricesService.h"

namespace aion::gameserver::handlers::ai::portals {

AION_AI(PortalRequestAI, "portal_request");

/** Java: the anonymous RequestResponseHandler<Npc> of handleUseItemFinish (PortalRequestAI.java:31-36); it captures the first location */
struct PortalRequestAI_Response final : model::gameobjects::player::RequestResponseHandler {
	AION_MAKE_REF_FRIEND
	const model::templates::teleport::TeleportLocation* const firstLoc;

	static runtime::Ref<PortalRequestAI_Response> create(Npc& owner, const model::templates::teleport::TeleportLocation* firstLoc) {
		return runtime::makeRef<PortalRequestAI_Response>(owner, firstLoc);
	}

	// Java PortalRequestAI.java:33-35
	void acceptRequest(runtime::Ptr<Creature>, Player& responder) override { // Java: Npc requester, unused
		TeleportService::teleport(responder, firstLoc, TeleportAnimation::JUMP_IN);
	}

protected:
	PortalRequestAI_Response(Npc& owner, const model::templates::teleport::TeleportLocation* firstLocValue)
		: RequestResponseHandler(runtime::Ptr<Creature>(owner)), firstLoc(firstLocValue) {}
	~PortalRequestAI_Response() override = default;
};

// Java PortalRequestAI.java:26-43 (getFirst() of an empty list: Java's NoSuchElementException)
void PortalRequestAI::handleUseItemFinish(Player& player) {
	const std::vector<model::templates::teleport::TeleportLocation>& locations = teleportTemplate.get()->getTeleLocIdData()->getTelelocations();
	if (locations.empty())
		throw runtime::NoSuchElementException("PortalRequestAI: no teleport location");
	const model::templates::teleport::TeleportLocation* firstLoc = &locations.front();
	const model::templates::teleport::TelelocationTemplate* locationTemplate =
		dataholders::DataManager::TELELOCATION_DATA->getTelelocationTemplate(firstLoc->getLocId());
	runtime::Ref<PortalRequestAI_Response> portal = PortalRequestAI_Response::create(getOwner(), firstLoc);
	int64_t transportationPrice = PricesService::getPriceForService(firstLoc->getPrice(), player.getRace());
	if (player.getResponseRequester().putRequest(SM_QUESTION_WINDOW::STR_TELEPORT_NEED_CONFIRM, portal)) {
		PacketSendUtility::sendPacket(player, SM_QUESTION_WINDOW(SM_QUESTION_WINDOW::STR_TELEPORT_NEED_CONFIRM, getObjectId(),
			getObjectTemplate()->getTalkDistance(), locationTemplate->getL10n(), transportationPrice));
	}
}

} // namespace aion::gameserver::handlers::ai::portals
