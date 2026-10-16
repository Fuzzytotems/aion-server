#include "aion/gameserver/services/craft/CraftSkillUpdateService.h"

#include <optional>
#include <string>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/configs/main/CraftConfig.h"
#include "aion/gameserver/model/craft/ProfessionInfo.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/RequestResponseHandler.h"
#include "aion/gameserver/model/gameobjects/player/ResponseRequester.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/model/skill/PlayerSkillList.h"
#include "aion/gameserver/network/aion/serverpackets/SM_QUESTION_WINDOW.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/services/item/ItemPacketService_ItemUpdateType.h"
#include "aion/gameserver/utils/PacketSendUtility.h"

namespace aion::gameserver::services::craft {

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.services.craft.CraftSkillUpdateService");

namespace {

using model::gameobjects::Creature;
using model::gameobjects::Npc;
using model::gameobjects::player::Player;
using model::gameobjects::player::RequestResponseHandler;
using network::aion::serverpackets::SM_QUESTION_WINDOW;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;
using utils::PacketSendUtility;

// fieldmap-class: com.aionemu.gameserver.services.craft.CraftSkillUpdateService$1
/**
 * Java: the anonymous RequestResponseHandler<Npc> of learnSkill (CraftSkillUpdateService.java:108-120), stored in the player's
 * ResponseRequester until he answers STR_CRAFT_ADDSKILL_CONFIRM. It captures the boxed price and the skill id and level; the requester (the
 * craft master) is the base's.
 */
class CraftSkillUpdateService_RequestResponseHandler final : public RequestResponseHandler {
	AION_MAKE_REF_FRIEND
public:
	const std::optional<int32_t> price; // captured local Integer price (line 95) [captured variable: final nullable scalar]
	const int32_t skillId;              // captured local int skillId (line 89) [captured variable: final scalar]
	const int32_t skillLevel;           // captured local int skillLevel (line 94) [captured variable: final scalar]

	static runtime::Ref<CraftSkillUpdateService_RequestResponseHandler> create(Npc& npc, std::optional<int32_t> priceValue, int32_t skillIdValue,
		int32_t skillLevelValue) {
		return runtime::makeRef<CraftSkillUpdateService_RequestResponseHandler>(npc, priceValue, skillIdValue, skillLevelValue);
	}

	// Java CraftSkillUpdateService.java:111-118
	void acceptRequest(runtime::Ptr<Creature> requesterValue, Player& responder) override {
		static_cast<void>(requesterValue);
		// Java unboxes the Integer; learnSkill never creates the handler for a null price, which would be its NullPointerException
		if (!price)
			throw runtime::NullPointerException("price");
		if (responder.getInventory().tryDecreaseKinah(*price, item::ItemPacketService_ItemUpdateType::DEC_KINAH_LEARN)) {
			runtime::Ptr<model::skill::PlayerSkillList> skillList = responder.getSkillList();
			skillList->addSkill(responder, skillId, skillLevel + 1);
		} else {
			PacketSendUtility::sendPacket(responder, SM_SYSTEM_MESSAGE::STR_NOT_ENOUGH_MONEY());
		}
	}

protected:
	CraftSkillUpdateService_RequestResponseHandler(Npc& npc, std::optional<int32_t> priceValue, int32_t skillIdValue, int32_t skillLevelValue)
		: RequestResponseHandler(runtime::Ptr<Creature>(npc)), price(priceValue), skillId(skillIdValue), skillLevel(skillLevelValue) {}
	~CraftSkillUpdateService_RequestResponseHandler() override = default;
};

} // namespace

CraftSkillUpdateService::CraftSkillUpdateService() {
	using model::craft::Profession;
	// Asmodian
	professionByNpc.put(204096, Profession::ESSENCETAPPING);
	professionByNpc.put(830150, Profession::ESSENCETAPPING);
	professionByNpc.put(204257, Profession::AETHERTAPPING);
	professionByNpc.put(830148, Profession::AETHERTAPPING);

	professionByNpc.put(204100, Profession::COOKING);
	professionByNpc.put(830142, Profession::COOKING);
	professionByNpc.put(204104, Profession::WEAPONSMITHING);
	professionByNpc.put(830146, Profession::WEAPONSMITHING);
	professionByNpc.put(204106, Profession::ARMORSMITHING);
	professionByNpc.put(830144, Profession::ARMORSMITHING);
	professionByNpc.put(204110, Profession::TAILORING);
	professionByNpc.put(830136, Profession::TAILORING);
	professionByNpc.put(204102, Profession::ALCHEMY);
	professionByNpc.put(830138, Profession::ALCHEMY);
	professionByNpc.put(204108, Profession::HANDICRAFTING);
	professionByNpc.put(830140, Profession::HANDICRAFTING);
	professionByNpc.put(798452, Profession::CONSTRUCTION);
	professionByNpc.put(798456, Profession::CONSTRUCTION);

	// Elyos
	professionByNpc.put(203780, Profession::ESSENCETAPPING);
	professionByNpc.put(830066, Profession::ESSENCETAPPING);
	professionByNpc.put(203782, Profession::AETHERTAPPING);
	professionByNpc.put(830064, Profession::AETHERTAPPING);

	professionByNpc.put(203784, Profession::COOKING);
	professionByNpc.put(830058, Profession::COOKING);
	professionByNpc.put(203788, Profession::WEAPONSMITHING);
	professionByNpc.put(830062, Profession::WEAPONSMITHING);
	professionByNpc.put(203790, Profession::ARMORSMITHING);
	professionByNpc.put(830060, Profession::ARMORSMITHING);
	professionByNpc.put(203793, Profession::TAILORING);
	professionByNpc.put(830052, Profession::TAILORING);
	professionByNpc.put(203786, Profession::ALCHEMY);
	professionByNpc.put(830054, Profession::ALCHEMY);
	professionByNpc.put(203792, Profession::HANDICRAFTING);
	professionByNpc.put(830056, Profession::HANDICRAFTING);
	professionByNpc.put(798450, Profession::CONSTRUCTION);
	professionByNpc.put(798454, Profession::CONSTRUCTION);

	log.info("CraftSkillUpdateService: Initialized.");
}

CraftSkillUpdateService::~CraftSkillUpdateService() = default;

CraftSkillUpdateService& CraftSkillUpdateService::getInstance() {
	static CraftSkillUpdateService instance; // Java SingletonHolder
	return instance;
}

std::optional<model::craft::Profession> CraftSkillUpdateService::getProfessionByNpc(model::gameobjects::Npc& npc) {
	return professionByNpc.get(npc.getNpcId());
}

// Java CraftSkillUpdateService.java:83-127; the handler is CraftSkillUpdateService_RequestResponseHandler above (fieldmap key
// CraftSkillUpdateService$1), stored in the player's ResponseRequester
void CraftSkillUpdateService::learnSkill(model::gameobjects::player::Player& player, model::gameobjects::Npc& npc) {
	if (player.getLevel() < 10)
		return;
	std::optional<model::craft::Profession> profession = professionByNpc.get(npc.getNpcId());
	if (!profession)
		return;
	int32_t skillId = model::craft::getSkillId(*profession);
	if (skillId == 0)
		return;

	runtime::Ptr<model::skill::PlayerSkillList> skillList = player.getSkillList();
	int32_t skillLevel = skillList->isSkillPresent(skillId) ? skillList->getSkillLevel(skillId) : 0;
	std::optional<int32_t> price = model::craft::getUpgradeCost(*profession, skillLevel);
	if (!price) {
		if (skillLevel > model::craft::getMaxUpgradableLevel(*profession))
			PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_DONT_RANK_UP_GATHERING());
		else if (skillLevel == 399) // expert is granted by quest
			PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_CRAFT_CANT_EXTEND_MONEY());
		else if (skillLevel == 499) // master is granted by quest
			PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_CRAFT_CANT_EXTEND_GRAND_MASTER());
		else
			PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_DONT_RANK_UP());
		return;
	}

	runtime::Ref<CraftSkillUpdateService_RequestResponseHandler> responseHandler =
		CraftSkillUpdateService_RequestResponseHandler::create(npc, price, skillId, skillLevel);

	if (player.getResponseRequester().putRequest(SM_QUESTION_WINDOW::STR_CRAFT_ADDSKILL_CONFIRM, responseHandler)) {
		std::string professionName =
			skillLevel == 0 ? model::craft::getClientName(*profession) : model::craft::getClientName(*profession, skillLevel + 1);
		// Java String.valueOf(price) of the Integer
		PacketSendUtility::sendPacket(player,
			SM_QUESTION_WINDOW(SM_QUESTION_WINDOW::STR_CRAFT_ADDSKILL_CONFIRM, 0, 0, professionName, std::to_string(*price)));
	}
}

int32_t CraftSkillUpdateService::getTotalExpertCraftingSkills(model::gameobjects::player::Player& player) {
	int32_t mastered = 0;

	for (model::craft::Profession profession : model::craft::PROFESSION_VALUES) {
		if (model::craft::isCrafting(profession) && player.getSkillList()->isSkillPresent(model::craft::getSkillId(profession))) {
			int32_t skillLvl = player.getSkillList()->getSkillLevel(model::craft::getSkillId(profession));
			if (skillLvl > 399 && skillLvl <= 499)
				mastered++;
		}
	}
	return mastered;
}

int32_t CraftSkillUpdateService::getTotalMasterCraftingSkills(model::gameobjects::player::Player& player) {
	int32_t mastered = 0;

	for (model::craft::Profession profession : model::craft::PROFESSION_VALUES) {
		if (model::craft::isCrafting(profession) && player.getSkillList()->isSkillPresent(model::craft::getSkillId(profession))) {
			int32_t skillLvl = player.getSkillList()->getSkillLevel(model::craft::getSkillId(profession));
			if (skillLvl > 499)
				mastered++;
		}
	}

	return mastered;
}

bool CraftSkillUpdateService::canLearnMoreExpertCraftingSkill(model::gameobjects::player::Player& player) {
	int32_t maxExpertCraftingSkills = configs::main::CraftConfig::MAX_EXPERT_CRAFTING_SKILLS.load();
	if (getTotalExpertCraftingSkills(player) + getTotalMasterCraftingSkills(player) < maxExpertCraftingSkills) {
		return true;
	} else {
		PacketSendUtility::sendMessage(player, "You can only be an expert in " + std::to_string(maxExpertCraftingSkills) + " professions.");
		return false;
	}
}

bool CraftSkillUpdateService::canLearnMoreMasterCraftingSkill(model::gameobjects::player::Player& player) {
	int32_t maxMasterCraftingSkills = configs::main::CraftConfig::MAX_MASTER_CRAFTING_SKILLS.load();
	if (getTotalMasterCraftingSkills(player) < maxMasterCraftingSkills) {
		return true;
	} else {
		PacketSendUtility::sendMessage(player, "You can only be a master in " + std::to_string(maxMasterCraftingSkills) + " professions.");
		return false;
	}
}

} // namespace aion::gameserver::services::craft
