#include "aion/gameserver/handlers/ai/siege/MercenaryAI.h"

#include <string>

#include "aion/gameserver/model/DialogAction.h"
#include "aion/gameserver/model/DialogPageInfo.h"
#include "aion/gameserver/model/gameobjects/siege/SiegeNpc.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/model/siege/FortressLocation.h"
#include "aion/gameserver/model/team/legion/Legion.h"
#include "aion/gameserver/network/aion/serverpackets/SM_DIALOG_WINDOW.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/services/SiegeService.h"
#include "aion/gameserver/services/siege/FortressSiege.h"
#include "aion/gameserver/services/siege/MercenaryLocation.h"

namespace aion::gameserver::handlers::ai::siege {

AION_AI(MercenaryAI, "mercenary");

// Java MercenaryAI.java:30-44 (Java's cast (SiegeNpc) getOwner(); getFortress(...).isVulnerable() on null is its NullPointerException)
void MercenaryAI::handleDialogStart(Player& player) {
	if (!player.isLegionMember()) {
		PacketSendUtility::sendPacket(player, SM_DIALOG_WINDOW(getObjectId(), 1011));
		return;
	}
	const int32_t siegeId = runtime::cast<SiegeNpc>(getOwner())->getSiegeId();
	runtime::Ptr<model::siege::FortressLocation> location = SiegeService::getInstance().getFortress(siegeId);
	if (location == nullptr)
		throw runtime::NullPointerException("FortressLocation " + std::to_string(siegeId));
	if (!location->isVulnerable())
		return;
	if (location->getLegionId() != player.getLegion()->getLegionId()) {
		PacketSendUtility::sendPacket(player, SM_DIALOG_WINDOW(getObjectId(), 1011));
		return;
	}
	PacketSendUtility::sendPacket(player, SM_DIALOG_WINDOW(getObjectId(), 10));
}

// Java MercenaryAI.java:46-174
bool MercenaryAI::onDialogSelect(Player& player, int32_t dialogActionId, int32_t questId, int32_t extendedRewardIndex) {
	static_cast<void>(questId);
	static_cast<void>(extendedRewardIndex);
	const int32_t siegeId = runtime::cast<SiegeNpc>(getOwner())->getSiegeId();
	int32_t zoneId = 0;
	switch (dialogActionId) {
		case model::DialogAction::SELECT1_2:
			PacketSendUtility::sendPacket(player, SM_DIALOG_WINDOW(getObjectId(), 1097));
			break;
		case model::DialogAction::SELECT1_3:
			PacketSendUtility::sendPacket(player, SM_DIALOG_WINDOW(getObjectId(), 1182));
			break;
		case model::DialogAction::SELECT1_4:
			PacketSendUtility::sendPacket(player, SM_DIALOG_WINDOW(getObjectId(), 1267));
			break;
		case model::DialogAction::SELECT2:
			PacketSendUtility::sendPacket(player, SM_DIALOG_WINDOW(getObjectId(), 1352));
			break;
		case model::DialogAction::SELECT3:
			PacketSendUtility::sendPacket(player, SM_DIALOG_WINDOW(getObjectId(), 1693));
			break;
		case model::DialogAction::SELECT4:
			PacketSendUtility::sendPacket(player, SM_DIALOG_WINDOW(getObjectId(), 2034));
			break;
		case model::DialogAction::SETPRO1:
			switch (siegeId) {
				case 1011:
				case 1221:
				case 1231:
				case 1241:
					zoneId = 1;
					break;
				case 2011:
				case 2021:
				case 3011:
				case 3021:
				case 7011:
					switch (getNpcId()) {
						case 832043: // 2011
						case 832059:
						case 832047: // 2021
						case 832063:
						case 832051: // 3011
						case 832067:
						case 832055: // 3021
						case 832071:
						case 804557: // 7011
						case 804558:
							zoneId = 1;
							break;
						case 832044: // 2011
						case 832060:
						case 832048: // 2021
						case 832064:
						case 832052: // 3011
						case 832068:
						case 832056: // 3021
						case 832072:
							zoneId = 2;
							break;
						case 832045: // 2011
						case 832061:
						case 832049: // 2021
						case 832065:
						case 832053: // 3011
						case 832069:
						case 832057: // 3021
						case 832073:
						case 802435: // 7011
						case 802436:
							zoneId = 3;
							break;
						case 832046: // 2011
						case 832062:
						case 832050: // 2021
						case 832066:
						case 832054: // 3011
						case 832070:
						case 832058: // 3021
						case 832074:
							zoneId = 4;
							break;
						case 804559:
						case 804560:
							zoneId = 5;
							break;
						default:
							break;
					}
					break;
				default:
					break;
			}
			break;
		case model::DialogAction::SETPRO2:
			switch (siegeId) {
				case 1011:
				case 1221:
				case 1231:
				case 1241:
					zoneId = 2;
					break;
				case 7011:
					switch (getNpcId()) {
						case 804557:
						case 804558:
							zoneId = 2;
							break;
						case 802435:
						case 802436:
							zoneId = 4;
							break;
						case 804559:
						case 804560:
							zoneId = 6;
							break;
						default:
							break;
					}
					break;
				default:
					break;
			}
			break;
		case model::DialogAction::SETPRO3:
			switch (siegeId) {
				case 1221:
				case 1231:
					zoneId = 3;
					break;
				case 7011: // Currently no npcid switch necessary
					zoneId = 7;
					break;
				default:
					break;
			}
			break;
		default:
			break;
	}
	checkMercenaryZone(player, siegeId, zoneId);
	return true;
}

// Java MercenaryAI.java:176-190 (Java's cast (FortressSiege) of the siege)
void MercenaryAI::checkMercenaryZone(Player& player, int32_t siegeId, int32_t zoneId) {
	runtime::Ptr<services::siege::FortressSiege> siege = runtime::cast<services::siege::FortressSiege>(SiegeService::getInstance().getSiege(siegeId));
	if (siege == nullptr)
		return;
	runtime::Ptr<services::siege::MercenaryLocation> mLoc = siege->getMercenaryLocationByZoneId(zoneId);
	if (mLoc == nullptr || !mLoc->isRequestValid())
		return;
	if (!player.getInventory().decreaseByItemId(186000236, mLoc->getCosts())) {
		PacketSendUtility::sendPacket(player, SM_DIALOG_WINDOW(getObjectId(), model::id(DialogPage::NO_RIGHT)));
		return;
	}
	PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE(mLoc->getMsgId(), std::vector<std::string>{}));
	PacketSendUtility::sendPacket(player, SM_DIALOG_WINDOW(getObjectId(), 2375));
	mLoc->spawn();
}

} // namespace aion::gameserver::handlers::ai::siege
