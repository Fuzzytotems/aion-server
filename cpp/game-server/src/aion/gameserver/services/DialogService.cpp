#include "aion/gameserver/services/DialogService.h"

#include <optional>
#include <string>
#include <vector>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/ai/AbstractAI.h"
#include "aion/gameserver/ai/event/AIEventType.h"
#include "aion/gameserver/configs/main/AutoGroupConfig.h"
#include "aion/gameserver/controllers/effect/PlayerEffectController.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/GoodsListData.h"
#include "aion/gameserver/dataholders/TradeListData.h"
#include "aion/gameserver/dataholders/loadingutils/EnumTraits.h"
#include "aion/gameserver/geoEngine/math/JavaFloat.h"
#include "aion/gameserver/model/DialogAction.h"
#include "aion/gameserver/model/DialogPage.h"
#include "aion/gameserver/model/DialogPageInfo.h"
#include "aion/gameserver/model/Race.h"
#include "aion/gameserver/model/RaceInfo.h"
#include "aion/gameserver/model/craft/Profession.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/PetAction.h"
#include "aion/gameserver/model/gameobjects/VisibleObject.h"
#include "aion/gameserver/model/gameobjects/player/AbyssRank.h"
#include "aion/gameserver/model/gameobjects/player/Mailbox.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"
#include "aion/gameserver/model/gameobjects/player/RequestResponseHandler.h"
#include "aion/gameserver/model/gameobjects/player/ResponseRequester.h"
#include "aion/gameserver/model/gameobjects/player/npcFaction/NpcFactions.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/model/siege/FortressLocation.h"
#include "aion/gameserver/model/siege/SiegeRace.h"
#include "aion/gameserver/model/siege/SiegeRaceInfo.h"
#include "aion/gameserver/model/skill/PlayerSkillEntry.h"
#include "aion/gameserver/model/skill/PlayerSkillList.h"
#include "aion/gameserver/model/team/TemporaryPlayerTeam.h"
#include "aion/gameserver/model/team/alliance/PlayerAlliance.h"
#include "aion/gameserver/model/team/legion/Legion.h"
#include "aion/gameserver/model/team/legion/LegionWarehouse.h"
#include "aion/gameserver/model/templates/goods/GoodsList.h"
#include "aion/gameserver/model/templates/npc/NpcTemplate.h"
#include "aion/gameserver/model/templates/npc/SubDialogType.h"
#include "aion/gameserver/model/templates/npc/TalkInfo.h"
#include "aion/gameserver/model/templates/tradelist/TradeListTemplate.h"
#include "aion/gameserver/model/templates/zone/ZoneClassName.h"
#include "aion/gameserver/model/templates/zone/ZoneTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_DIALOG_WINDOW.h"
#include "aion/gameserver/network/aion/serverpackets/SM_PET.h"
#include "aion/gameserver/network/aion/serverpackets/SM_PLASTIC_SURGERY.h"
#include "aion/gameserver/network/aion/serverpackets/SM_QUESTION_WINDOW.h"
#include "aion/gameserver/network/aion/serverpackets/SM_REPURCHASE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SELL_ITEM.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_TRADELIST.h"
#include "aion/gameserver/network/aion/serverpackets/SM_TRADE_IN_LIST.h"
#include "aion/gameserver/questEngine/QuestEngine.h"
#include "aion/gameserver/questEngine/model/QuestEnv.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/services/CubeExpandService.h"
#include "aion/gameserver/services/HousingService.h"
#include "aion/gameserver/services/LegionDominionService.h"
#include "aion/gameserver/services/LegionService.h"
#include "aion/gameserver/services/SiegeService.h"
#include "aion/gameserver/services/WarehouseService.h"
#include "aion/gameserver/services/abyss/AbyssRankingCache.h"
#include "aion/gameserver/services/craft/CraftSkillUpdateService.h"
#include "aion/gameserver/services/craft/RelinquishCraftStatus.h"
#include "aion/gameserver/services/item/ItemChargeService.h"
#include "aion/gameserver/services/item/ItemPacketService_ItemUpdateType.h"
#include "aion/gameserver/services/player/PlayerMailboxState.h"
#include "aion/gameserver/services/teleport/TeleportService.h"
#include "aion/gameserver/services/trade/PricesService.h"
#include "aion/gameserver/skillengine/effect/SummonOwner.h"
#include "aion/gameserver/skillengine/model/DispelSlotType.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/utils/stats/AbyssRankEnum.h"
#include "aion/gameserver/world/zone/ZoneInstance.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::services {

using model::gameobjects::Creature;
using model::gameobjects::Npc;
using model::gameobjects::player::Player;
using model::gameobjects::player::RequestResponseHandler;
using network::aion::serverpackets::SM_DIALOG_WINDOW;
using network::aion::serverpackets::SM_QUESTION_WINDOW;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;
using player::PlayerMailboxState;
using utils::PacketSendUtility;

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.services.DialogService");

namespace {

/** Java int multiplication: wraps on overflow (signed overflow is undefined in C++) */
int32_t javaIntMul(int32_t a, int32_t b) {
	return static_cast<int32_t>(static_cast<uint32_t>(a) * static_cast<uint32_t>(b));
}

/** Java AbyssRankEnum.getId(): the ordinal + 1 (GRADE9_SOLDIER 1 .. SUPREME_COMMANDER 18, AbyssRankEnum.java:15-32) */
int32_t abyssRankId(utils::stats::AbyssRankEnum rank) {
	return static_cast<int32_t>(rank) + 1;
}

// fieldmap-class: com.aionemu.gameserver.services.DialogService$1
/**
 * Java: the anonymous RequestResponseHandler<Npc> of the RECOVERY arm (DialogService.java:135-150): soul healing, the answer to
 * SM_QUESTION_WINDOW.STR_ASK_RECOVER_EXPERIENCE. It captures the two final locals of the arm; the requester (the healer npc) is the base's.
 */
class DialogService_RequestResponseHandler final : public RequestResponseHandler {
	AION_MAKE_REF_FRIEND
public:
	const int32_t price;   // local int price (line 133) [captured variable: final scalar]
	const int64_t expLost; // local long expLost (line 127) [captured variable: final scalar]

	static runtime::Ref<DialogService_RequestResponseHandler> create(Npc& npc, int32_t priceValue, int64_t expLostValue) {
		return runtime::makeRef<DialogService_RequestResponseHandler>(npc, priceValue, expLostValue);
	}

	// Java DialogService.java:137-149
	void acceptRequest(runtime::Ptr<Creature> requesterValue, Player& responder) override {
		static_cast<void>(requesterValue);
		if (responder.getInventory().getKinah() >= price) {
			PacketSendUtility::sendPacket(responder, SM_SYSTEM_MESSAGE::STR_GET_EXP2(expLost));
			PacketSendUtility::sendPacket(responder, SM_SYSTEM_MESSAGE::STR_SUCCESS_RECOVER_EXPERIENCE());
			responder.getCommonData()->resetRecoverableExp();
			responder.getInventory().decreaseKinah(price, item::ItemPacketService_ItemUpdateType::STATS_CHANGE);
			responder.getEffectController()->removeByDispelSlotType(skillengine::model::DispelSlotType::SPECIAL2);
			responder.getCommonData()->setDeathCount(0);
		} else {
			PacketSendUtility::sendPacket(responder, SM_SYSTEM_MESSAGE::STR_MSG_NOT_ENOUGH_KINA(price));
		}
	}

protected:
	DialogService_RequestResponseHandler(Npc& npc, int32_t priceValue, int64_t expLostValue)
		: RequestResponseHandler(runtime::Ptr<Creature>(npc)), price(priceValue), expLost(expLostValue) {}
	~DialogService_RequestResponseHandler() override = default;
};

} // namespace

// Java DialogService.java:53-67
void DialogService::onCloseDialog(Player& player, runtime::Ptr<model::gameobjects::VisibleObject> target) {
	if (!target)
		return;

	if (runtime::Ptr<Npc> npc = runtime::as<Npc>(target)) {
		npc->getAi().onCreatureEvent(ai::event::AIEventType::DIALOG_FINISH, player);

		if (npc->getObjectTemplate()->supportsAction(model::DialogAction::OPEN_LEGION_WAREHOUSE) && player.isLegionMember())
			player.getLegion()->getLegionWarehouse().unsetInUse(player.getObjectId());
	}

	runtime::Ptr<model::gameobjects::player::Mailbox> mailbox = player.getMailbox();
	if (mailbox && mailbox->mailBoxState.get() != PlayerMailboxState::CLOSED)
		mailbox->mailBoxState.set(PlayerMailboxState::CLOSED);
}

// Java DialogService.java:69-280
void DialogService::onDialogSelect(int32_t dialogActionId, Player& player, Npc& npc, int32_t questId, int32_t extendedRewardIndex) {
	using namespace model::DialogAction;
	int32_t targetObjectId = npc.getObjectId();

	if (questId == 0) {
		switch (dialogActionId) {
			case BUY: {
				const model::templates::tradelist::TradeListTemplate* tradeListTemplate =
					dataholders::DataManager::TRADE_LIST_DATA->getTradeListTemplate(npc.getNpcId());
				if (tradeListTemplate == nullptr) {
					PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_BUY_SELL_HE_DOES_NOT_SELL_ITEM(npc.getObjectTemplate()->getL10n()));
					break;
				}
				int32_t tradeModifier = tradeListTemplate->getSellPriceRate();
				// Java reads player.getLegion() twice; the second read only runs when the first was not null
				int32_t legionLevel = !player.getLegion() ? 0 : player.getLegion()->getLegionLevel();
				bool hasAnythingToSell = false;
				for (const model::templates::tradelist::TradeListTemplate::TradeTab& tab : tradeListTemplate->getTradeTablist()) {
					const model::templates::goods::GoodsList* goodsList = dataholders::DataManager::GOODSLIST_DATA->getGoodsListById(tab.getId());
					if (goodsList == nullptr || goodsList->getLegionLevel() > legionLevel)
						continue;
					hasAnythingToSell = true;
					break;
				}
				if (hasAnythingToSell)
					PacketSendUtility::sendPacket(player, network::aion::serverpackets::SM_TRADELIST(player, npc, tradeListTemplate,
															  javaIntMul(trade::PricesService::getVendorBuyModifier(), tradeModifier) / 100));
				else
					PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_BUY_SELL_HE_DOES_NOT_SELL_ITEM(npc.getObjectTemplate()->getL10n()));
				break;
			}
			case DEPOSIT_CHAR_WAREHOUSE: // warehouse (2.5)
			case OPEN_VENDOR: // Consign trade?? npc karinerk, koorunerk (2.5)
			case OPEN_STIGMA_WINDOW: // stigma
			case CREATE_LEGION: // create legion
			case OPEN_STIGMA_ENCHANT:
			case GIVE_ITEM_PROC: // Godstone socketing (2.5)
			case REMOVE_ITEM_OPTION: // remove manastone (2.5)
			case CHANGE_ITEM_SKIN: // modify appearance (2.5)
			case ITEM_UPGRADE: // item upgrade (4.7)
			case CLOSE_LEGION_WAREHOUSE: // WTF??? Quest dialog packet (2.5)
			case COMBINE_TASK: // crafting (2.5)
			case OPEN_INSTANCE_RECRUIT: // handled by AI
			case INSTANCE_ENTRY: // (2.5)
			case COMPOUND_WEAPON: // armsfusion (2.5)
			case DECOMPOUND_WEAPON: // armsbreaking (2.5)
			case HOUSING_BUILD: // housing build
			case HOUSING_DESTRUCT: // housing destruct
			case HOUSING_PERSONAL_AUCTION: // housing auction
			case CHARGE_ITEM_SINGLE: // condition an individual item
			case CHARGE_ITEM_SINGLE2: // augmenting an individual item
			case TOWN_CHALLENGE: // town improvement
				sendDialogWindow(dialogActionId, player, npc);
				break;
			case DISPERSE_LEGION: // disband legion
				LegionService::getInstance().requestDisbandLegion(npc, player);
				break;
			case RECREATE_LEGION: // recreate legion
				LegionService::getInstance().recreateLegion(npc, player);
				break;
			case RECOVERY: { // soul healing (2.5)
				const int64_t expLost = player.getCommonData()->getExpRecoverable();
				if (expLost == 0) {
					player.getEffectController()->removeByDispelSlotType(skillengine::model::DispelSlotType::SPECIAL2);
					player.getCommonData()->setDeathCount(0);
				}
				const double factor = (expLost < 1000000 ? 0.25 - (0.00000015 * static_cast<double>(expLost)) : 0.1);
				const int32_t price = geoEngine::math::JavaFloat::doubleToInt(static_cast<double>(expLost) * factor);

				runtime::Ref<DialogService_RequestResponseHandler> responseHandler = DialogService_RequestResponseHandler::create(npc, price, expLost);
				if (player.getCommonData()->getExpRecoverable() > 0) {
					bool result = player.getResponseRequester().putRequest(SM_QUESTION_WINDOW::STR_ASK_RECOVER_EXPERIENCE,
						runtime::Ptr<RequestResponseHandler>(responseHandler));
					if (result) {
						PacketSendUtility::sendPacket(player,
							SM_QUESTION_WINDOW(SM_QUESTION_WINDOW::STR_ASK_RECOVER_EXPERIENCE, 0, 0, SM_QUESTION_WINDOW::toJavaString(price)));
					}
				} else {
					PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_DONOT_HAVE_RECOVER_EXPERIENCE());
				}
				break;
			}
			case ENTER_PVP: // (2.5)
				switch (npc.getNpcId()) {
					case 204089: // pvp arena in pandaemonium.
						teleport::TeleportService::teleportTo(player, 120010000, 1, 984.0f, 1543.0f, 222.1f);
						break;
					case 203764: // pvp arena in sanctum.
						teleport::TeleportService::teleportTo(player, 110010000, 1, 1462.5f, 1326.1f, 564.1f);
						break;
					case 203981:
						teleport::TeleportService::teleportTo(player, 210020000, 1, 439.3f, 422.2f, 274.3f);
						break;
				}
				break;
			case LEAVE_PVP: // (2.5)
				switch (npc.getNpcId()) {
					case 204087:
						teleport::TeleportService::teleportTo(player, 120010000, 1, 1005.1f, 1528.9f, 222.1f);
						break;
					case 203875:
						teleport::TeleportService::teleportTo(player, 110010000, 1, 1470.3f, 1343.5f, 563.7f);
						break;
					case 203982:
						teleport::TeleportService::teleportTo(player, 210020000, 1, 446.2f, 431.1f, 274.5f);
						break;
				}
				break;
			case AIRLINE_SERVICE: { // flight and teleport (2.5)
				switch (npc.getNpcId()) {
					case 203679: // ishalgen teleporter
					case 203194: // poeta teleporter
						if (!player.getCommonData()->isDaeva()) {
							PacketSendUtility::sendPacket(player, SM_DIALOG_WINDOW(npc.getObjectId(), model::id(model::DialogPage::NO_RIGHT)));
							return;
						}
				}
				teleport::TeleportService::showMap(player, npc);
				break;
			}
			case GATHER_SKILL_LEVELUP: // improve extraction (2.5)
			case COMBINE_SKILL_LEVELUP: // learn tailoring armor smithing etc. (2.5)
				craft::CraftSkillUpdateService::getInstance().learnSkill(player, npc);
				break;
			case EXTEND_INVENTORY: // expand cube (2.5)
				CubeExpandService::expandCube(player, npc);
				break;
			case EXTEND_CHAR_WAREHOUSE: // (2.5)
				WarehouseService::expandWarehouse(player, npc);
				break;
			case OPEN_LEGION_WAREHOUSE: // legion warehouse (2.5)
				LegionService::getInstance().openLegionWarehouse(player, npc);
				break;
			case EDIT_CHARACTER_ALL:
			case EDIT_CHARACTER_GENDER: // (2.5) and (4.3)
				PacketSendUtility::sendPacket(player, network::aion::serverpackets::SM_PLASTIC_SURGERY(player, dialogActionId == EDIT_CHARACTER_GENDER));
				player.getCommonData()->setInEditMode(true);
				break;
			case MATCH_MAKER: // dredgion
				if (configs::main::AutoGroupConfig::AUTO_GROUP_ENABLE.load()) {
					// Java: AutoGroupType agt = AutoGroupType.getAutoGroup(npc.getNpcId()); if (agt != null &&
					// PeriodicInstanceManager.getInstance().isRegistrationOpen(agt.getTemplate().getMaskId())) sendPacket(new SM_AUTO_GROUP(maskId)).
					// AutoGroupType's constructor data and getAutoGroup/getTemplate have no C++ companion yet (only the generated constants,
					// model/autogroup/AutoGroupType.h), so the autogroup-on branch stays loud until the autogroup milestone (m5c-plan.md W-31, D4).
					// Every gate profile and the shipped mygs.properties set gameserver.autogroup.enable = false.
					AION_UNPORTED();
				} else {
					PacketSendUtility::sendPacket(player, SM_DIALOG_WINDOW(targetObjectId, 1011));
				}
				break;
			case FACTION_JOIN: // join npcFaction (2.5)
				player.getNpcFactions().enterGuild(npc);
				break;
			case FACTION_SEPARATE: // leave npcFaction (2.5)
				player.getNpcFactions().leaveNpcFaction(npc);
				break;
			case BUY_AGAIN: // repurchase (2.5)
				PacketSendUtility::sendPacket(player, network::aion::serverpackets::SM_REPURCHASE(player, npc.getObjectId()));
				break;
			case FUNC_PET_ADOPT: // adopt pet (2.5)
				PacketSendUtility::sendPacket(player, network::aion::serverpackets::SM_PET(model::gameobjects::PetAction::TALK_WITH_MERCHANT));
				break;
			case FUNC_PET_ABANDON: // surrender pet (2.5)
				PacketSendUtility::sendPacket(player, network::aion::serverpackets::SM_PET(model::gameobjects::PetAction::TALK_WITH_MINDER));
				break;
			case CHARGE_ITEM_MULTI: // condition all equiped items
				item::ItemChargeService::startChargingEquippedItems(player, targetObjectId, 1);
				break;
			case TRADE_IN: {
				const model::templates::tradelist::TradeListTemplate* tradeListTemplate =
					dataholders::DataManager::TRADE_LIST_DATA->getTradeInListTemplate(npc.getNpcId());
				if (tradeListTemplate == nullptr)
					PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_BUY_SELL_HE_DOES_NOT_SELL_ITEM(npc.getObjectTemplate()->getL10n()));
				else
					PacketSendUtility::sendPacket(player, network::aion::serverpackets::SM_TRADE_IN_LIST(npc, tradeListTemplate, 100));
				break;
			}
			case SELL:
			case TRADE_SELL_LIST:
				PacketSendUtility::sendPacket(player, network::aion::serverpackets::SM_SELL_ITEM(npc));
				break;
			case GIVEUP_CRAFT_EXPERT: { // relinquish Expert Status
				// Java passes getProfessionByNpc's null on, and relinquishCraftStatus answers false for it before anything else
				// (RelinquishCraftStatus.java:47); the C++ Profession parameter is not nullable, so a null profession skips the call instead
				std::optional<model::craft::Profession> profession = craft::CraftSkillUpdateService::getInstance().getProfessionByNpc(npc);
				if (profession)
					craft::RelinquishCraftStatus::relinquishExpertStatus(player, *profession);
				break;
			}
			case GIVEUP_CRAFT_MASTER: { // relinquish Master Status
				std::optional<model::craft::Profession> profession = craft::CraftSkillUpdateService::getInstance().getProfessionByNpc(npc);
				if (profession) // as GIVEUP_CRAFT_EXPERT
					craft::RelinquishCraftStatus::relinquishMasterStatus(player, *profession);
				break;
			}
			case FUNC_PET_H_ADOPT:
				PacketSendUtility::sendPacket(player, network::aion::serverpackets::SM_PET(model::gameobjects::PetAction::H_ADOPT));
				break;
			case FUNC_PET_H_ABANDON:
				PacketSendUtility::sendPacket(player, network::aion::serverpackets::SM_PET(model::gameobjects::PetAction::H_ABANDON));
				break;
			case CHARGE_ITEM_MULTI2: // augmenting all equipped items
				item::ItemChargeService::startChargingEquippedItems(player, targetObjectId, 2);
				break;
			case HOUSING_RECREATE_PERSONAL_INS: // recreate personal house instance (studio)
				HousingService::getInstance().recreatePlayerStudio(player);
				break;
			default:
				handleQuestDialogueOrSendNextPage(dialogActionId, player, npc, questId, extendedRewardIndex);
				break;
		}
	} else {
		handleQuestDialogueOrSendNextPage(dialogActionId, player, npc, questId, extendedRewardIndex);
	}
}

// Java DialogService.java:282-291
void DialogService::handleQuestDialogueOrSendNextPage(int32_t dialogActionId, Player& player, Npc& npc, int32_t questId,
	int32_t extendedRewardIndex) {
	if (questId != 0 || dialogActionId == model::DialogAction::USE_OBJECT || dialogActionId == model::DialogAction::EXCHANGE_COIN) {
		runtime::Ref<questEngine::model::QuestEnv> env =
			questEngine::model::QuestEnv::create(runtime::Ptr<model::gameobjects::VisibleObject>(npc), player, questId, dialogActionId);
		env->setExtendedRewardIndex(extendedRewardIndex);
		if (questEngine::QuestEngine::getInstance().onDialog(*env))
			return;
	}
	// action id = next page id
	PacketSendUtility::sendPacket(player, SM_DIALOG_WINDOW(npc.getObjectId(), dialogActionId, questId));
}

// Java DialogService.java:293-296
void DialogService::sendDialogWindow(int32_t dialogActionId, Player& player, Npc& npc) {
	if (npc.getObjectTemplate()->supportsAction(dialogActionId))
		PacketSendUtility::sendPacket(player, SM_DIALOG_WINDOW(npc.getObjectId(), model::id(model::getByActionId(dialogActionId))));
}

// Java DialogService.java:298-302
bool DialogService::isInteractionAllowed(Player& player, Npc& npc) {
	if (npc.getSummonOwner() && !isSummonOwner(player, npc))
		return false;
	return !isSubDialogRestricted(player, npc);
}

// Java DialogService.java:304-312
bool DialogService::isSummonOwner(Player& player, Npc& npc) {
	bool playerIsCreator = player.getObjectId() == npc.getCreatorId();
	std::optional<skillengine::effect::SummonOwner> summonOwner = npc.getSummonOwner();
	if (!summonOwner) // Java: a switch on a null enum is a NullPointerException (isInteractionAllowed checks it first)
		throw runtime::NullPointerException("Cannot invoke \"SummonOwner.ordinal()\" because the return value of \"Npc.getSummonOwner()\" is null");
	switch (*summonOwner) {
		case skillengine::effect::SummonOwner::PRIVATE:
			return playerIsCreator;
		case skillengine::effect::SummonOwner::GROUP:
			return playerIsCreator || (player.getCurrentGroup() && player.getCurrentGroup()->hasMember(npc.getCreatorId()));
		case skillengine::effect::SummonOwner::ALLIANCE: {
			if (playerIsCreator)
				return true;
			runtime::Ptr<model::team::alliance::PlayerAlliance> alliance = runtime::as<model::team::alliance::PlayerAlliance>(player.getCurrentTeam());
			return alliance && alliance->hasMember(npc.getCreatorId());
		}
		case skillengine::effect::SummonOwner::LEGION:
			return playerIsCreator || (player.isLegionMember() && player.getLegion()->isMember(npc.getCreatorId()));
	}
	// Java: the switch expression is exhaustive; an unknown constant would be a MatchException
	throw runtime::IllegalStateException("Unexpected SummonOwner " + std::string(xml::enumName(*summonOwner)));
}

// Java DialogService.java:314-377
bool DialogService::isSubDialogRestricted(Player& player, Npc& npc) {
	using model::templates::npc::SubDialogType;
	const model::templates::npc::TalkInfo* talkInfo = npc.getObjectTemplate()->getTalkInfo();
	if (talkInfo == nullptr || !talkInfo->getSubDialogType())
		return false;
	switch (*talkInfo->getSubDialogType()) {
		case SubDialogType::FORT_CAPTURE: {
			if (!player.getLegion())
				return true;
			const model::templates::zone::ZoneTemplate* fortZone = nullptr;
			for (const runtime::Ptr<world::zone::ZoneInstance>& zone : npc.findZones()) {
				if (zone->getZoneTemplate()->getZoneType() != model::templates::zone::ZoneClassName::FORT)
					continue;
				fortZone = zone->getZoneTemplate();
				break;
			}
			if (fortZone == nullptr) {
				log.warn("Could not find FORT zone for npc: " + std::to_string(npc.getNpcId()));
				return true;
			}
			const std::optional<std::vector<int32_t>>& siegeIds = fortZone->getSiegeId();
			if (!siegeIds) // Java: the for-each over a null List is a NullPointerException
				throw runtime::NullPointerException("Cannot invoke \"java.util.List.iterator()\" because \"siegeIds\" is null");
			for (int32_t siegeId : *siegeIds) {
				runtime::Ptr<model::siege::FortressLocation> loc = SiegeService::getInstance().getFortress(siegeId);
				if (!loc)
					continue;
				if (model::siege::getRaceId(loc->getRace()) == model::getRaceId(player.getRace())
					&& loc->getLegionId() == player.getLegion()->getLegionId()) {
					return false;
				}
			}
			return true;
		}
		case SubDialogType::SKILL_ID:
			return !player.getSkillList()->getSkillEntry(talkInfo->getSubDialogValue());
		case SubDialogType::ITEM_ID:
			return player.getInventory().getItemCountByItemId(talkInfo->getSubDialogValue()) == 0;
		case SubDialogType::RETURN:
			return player.getInventory().getItemCountByItemId(164000335) == 0; // Abbey Return Stone (30 days)
		case SubDialogType::ABYSSRANK:
			if (player.isStaff())
				return false;
			return abyssRankId(player.getAbyssRank()->getRank()) < talkInfo->getSubDialogValue();
		case SubDialogType::ABYSSRANKING:
			return abyss::AbyssRankingCache::getInstance().getRankingListPosition(player) > talkInfo->getSubDialogValue();
		case SubDialogType::TARGET_LEGION_DOMINION:
			if (LegionDominionService::getInstance().isInCalculationTime())
				return true;
			if (player.getLegion()) {
				return player.getLegion()->getCurrentLegionDominion() != talkInfo->getSubDialogValue();
			}
			return true;
		case SubDialogType::LEGION_DOMINION_NPC:
			if (player.getLegion()) {
				return player.getLegion()->getOccupiedLegionDominion() != talkInfo->getSubDialogValue();
			}
			return true;
		case SubDialogType::LEVEL:
			return player.getLevel() != talkInfo->getSubDialogValue();
		case SubDialogType::LEVEL_LOW:
			return player.getLevel() > talkInfo->getSubDialogValue();
		case SubDialogType::LEVEL_HIGH:
			return player.getLevel() < talkInfo->getSubDialogValue();
		default:
			log.warn("Unhandled subdialog type " + std::string(xml::enumName(*talkInfo->getSubDialogType())) + " for npc: "
				+ std::to_string(npc.getNpcId()));
			return true;
	}
}

} // namespace aion::gameserver::services
