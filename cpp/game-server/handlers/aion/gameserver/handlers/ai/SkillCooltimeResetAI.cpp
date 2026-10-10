#include "aion/gameserver/handlers/ai/SkillCooltimeResetAI.h"

#include <algorithm>
#include <iterator>
#include <unordered_map>
#include <vector>

#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/ai/AIActions.h"
#include "aion/gameserver/ai/AIRequest.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/controllers/PlayerController.h"
#include "aion/gameserver/custom/pvpmap/PvpMapService.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/SkillData.h"
#include "aion/gameserver/model/TaskId.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/TransformModel.h"
#include "aion/gameserver/model/items/ItemCooldown.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/model/skill/PlayerSkillEntry.h"
#include "aion/gameserver/model/skill/PlayerSkillList.h"
#include "aion/gameserver/model/stats/container/PlayerLifeStats.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/model/templates/item/ItemUseLimits.h"
#include "aion/gameserver/model/templates/item/actions/ItemActions.h"
#include "aion/gameserver/model/templates/item/actions/SkillUseAction.h"
#include "aion/gameserver/model/templates/npc/NpcTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ATTACK_STATUS.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ITEM_COOLDOWN.h"
#include "aion/gameserver/network/aion/serverpackets/SM_MESSAGE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SKILL_COOLDOWN.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/skillengine/model/SkillTemplate.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/utils/PositionUtil.h"
#include "aion/gameserver/world/geo/GeoService.h"
#include "aion/gameserver/world/knownlist/KnownList.h"

namespace aion::gameserver::handlers::ai {

using network::aion::serverpackets::SM_ATTACK_STATUS_LOG;
using network::aion::serverpackets::SM_ATTACK_STATUS_TYPE;
using network::aion::serverpackets::SM_ITEM_COOLDOWN;
using network::aion::serverpackets::SM_MESSAGE;
using network::aion::serverpackets::SM_SKILL_COOLDOWN;

AION_AI(SkillCooltimeResetAI, "customcdreset");

/**
 * Java: the anonymous AIRequest of sendRequest (SkillCooltimeResetAI.java:93-134, fieldmap ai.SkillCooltimeResetAI$1), stored through
 * AIActions.addRequest in the player's ResponseRequester until he answers 1300765. It captures the AI, whose getOwner() is the npc.
 */
struct SkillCooltimeResetAI_AIRequest final : AIRequest {
	AION_MAKE_REF_FRIEND

	const runtime::Ref<SkillCooltimeResetAI> skillCooltimeResetAI; // captured this (SkillCooltimeResetAI.java:109)

	static runtime::Ref<SkillCooltimeResetAI_AIRequest> create(SkillCooltimeResetAI& aiValue) {
		return runtime::makeRef<SkillCooltimeResetAI_AIRequest>(aiValue);
	}

	// Java SkillCooltimeResetAI.java:96-132
	void acceptRequest(runtime::Ptr<Creature> requester, Player& responder, int32_t requestId) override {
		static_cast<void>(requester);
		static_cast<void>(requestId);
		if (responder.getInventory().getKinah() < SkillCooltimeResetAI::PRICE)
			PacketSendUtility::sendPacket(responder, SM_SYSTEM_MESSAGE::STR_MSG_NOT_ENOUGH_KINA(SkillCooltimeResetAI::PRICE));
		else if (responder.getLifeStats()->isAboutToDie() || responder.isDead())
			PacketSendUtility::sendPacket(responder, SM_SYSTEM_MESSAGE::STR_CANNOT_USE_IN_DEAD_STATE());
		else if (responder.getController().isInCombat())
			PacketSendUtility::sendPacket(responder, SM_SYSTEM_MESSAGE::STR_SKILL_CANT_CAST_IN_COMBAT_STATE());
		else if (responder.isTransformed() && responder.getTransformModel().getType() == TransformType::AVATAR)
			PacketSendUtility::sendPacket(responder, SM_SYSTEM_MESSAGE::STR_SKILL_CAN_NOT_ACT_WHILE_IN_ABNORMAL_STATE());
		else {
			std::set<int32_t> skillCooldownIds = skillCooltimeResetAI->collectResettableSkillCooldownIds(responder);
			std::set<int32_t> itemCooldownIds = skillCooltimeResetAI->collectResettableItemCooldownIds(responder);
			if (!skillCooldownIds.empty() || !itemCooldownIds.empty()) {
				if (responder.getInventory().tryDecreaseKinah(SkillCooltimeResetAI::PRICE)) {
					responder.getLifeStats()->increaseHp(SM_ATTACK_STATUS_TYPE::HP, responder.getLifeStats()->getMaxHp(), skillCooltimeResetAI->getOwner());
					responder.getLifeStats()->increaseMp(SM_ATTACK_STATUS_TYPE::HEAL_MP, responder.getLifeStats()->getMaxMp(), 0,
						SM_ATTACK_STATUS_LOG::MPHEAL);
					if (!skillCooldownIds.empty()) {
						for (int32_t cooldownId : skillCooldownIds)
							responder.removeSkillCoolDown(cooldownId);
						PacketSendUtility::sendPacket(responder,
							SM_SKILL_COOLDOWN(responder, std::vector<int32_t>(skillCooldownIds.begin(), skillCooldownIds.end())));
					}
					if (!itemCooldownIds.empty()) {
						// 4.8 client ignores reuseTime <= currentTime, but sending old cds + useDelay 0 works
						std::unordered_map<int32_t, runtime::Ptr<ItemCooldown>> dummyCds;
						std::vector<runtime::Ref<ItemCooldown>> keepAlive;
						for (int32_t itemCooldownId : itemCooldownIds) {
							runtime::Ref<ItemCooldown> dummy = ItemCooldown::create(responder.getItemReuseTime(itemCooldownId), 0);
							dummyCds[itemCooldownId] = runtime::Ptr<ItemCooldown>(dummy);
							keepAlive.push_back(dummy);
							responder.removeItemCoolDown(itemCooldownId);
						}
						PacketSendUtility::sendPacket(responder, SM_ITEM_COOLDOWN(dummyCds));
					}
					if (PvpMapService::getInstance().isOnPvPMap(skillCooltimeResetAI->getOwner())) {
						skillCooltimeResetAI->getOwner().getController().delete_();
					}
				} else {
					PacketSendUtility::sendPacket(responder, SM_SYSTEM_MESSAGE::STR_MSG_NOT_ENOUGH_KINA(SkillCooltimeResetAI::PRICE));
				}
			}
		}
	}

protected:
	explicit SkillCooltimeResetAI_AIRequest(SkillCooltimeResetAI& aiValue) : skillCooltimeResetAI(aiValue) {}
	~SkillCooltimeResetAI_AIRequest() override = default;
};

// Java SkillCooltimeResetAI.java:50-57. Both tasks are pinned on this AI (a part of its npc).
void SkillCooltimeResetAI::handleSpawned() {
	NpcAI::handleSpawned();
	if (PvpMapService::getInstance().isOnPvPMap(getOwner())) {
		getOwner().getController().addTask(TaskId::DESPAWN,
			ThreadPoolManager::getInstance().schedule({this}, [this] { getOwner().getController().delete_(); }, 30000));
		ThreadPoolManager::getInstance().schedule({this}, [this] { getOwner().getKnownList().forEachPlayer([this](Player& player) { tryNotify(player); }); },
			1000);
	}
}

// Java SkillCooltimeResetAI.java:59-73
void SkillCooltimeResetAI::handleDialogStart(Player& player) {
	playersInSight.values().removeIf([](int64_t time) { return commons::utils::currentTimeMillis() > time + 300000; }); // remove players if they are already 5 mins+ in the map
	if (player.getLifeStats()->isAboutToDie() || player.isDead())
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_CANNOT_USE_IN_DEAD_STATE());
	else if (!PvpMapService::getInstance().isOnPvPMap(player) && player.getController().isInCombat())
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_SKILL_CANT_CAST_IN_COMBAT_STATE());
	else if (player.isTransformed() && player.getTransformModel().getType() == TransformType::AVATAR)
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_SKILL_CAN_NOT_ACT_WHILE_IN_ABNORMAL_STATE());
	else if (collectResettableSkillCooldownIds(player).empty() && collectResettableItemCooldownIds(player).empty())
		PacketSendUtility::sendPacket(player, SM_MESSAGE(getOwner(), "Daeva has no skill cooldowns to reset, yang.", ChatType::NPC));
	else
		sendRequest(player);
}

// Java SkillCooltimeResetAI.java:75-79
void SkillCooltimeResetAI::handleCreatureMoved(Creature& creature) {
	if (Player* player = dynamic_cast<Player*>(&creature))
		tryNotify(*player);
}

// Java SkillCooltimeResetAI.java:81-92
void SkillCooltimeResetAI::tryNotify(Player& player) {
	if (player.isDead())
		return;
	if (!getOwner().canSee(runtime::Ptr<VisibleObject>(player)))
		return;
	if (playersInSight.containsKey(player.getObjectId()))
		return;
	if (PositionUtil::isInRange(getOwner(), player, 8) && GeoService::getInstance().canSee(getOwner(), player)) {
		playersInSight.put(player.getObjectId(), commons::utils::currentTimeMillis());
		PacketSendUtility::sendPacket(player,
			SM_MESSAGE(getOwner(), "I can heal you and reset your skill cooldowns for " + formatGrouped(PRICE) + " Kinah, yang yang.", ChatType::NPC));
	}
}

// Java SkillCooltimeResetAI.java:94-135
void SkillCooltimeResetAI::sendRequest(Player& player) {
	runtime::Ref<SkillCooltimeResetAI_AIRequest> request = SkillCooltimeResetAI_AIRequest::create(*this);
	AIActions::addRequest(*this, player, 1300765, getObjectTemplate()->getTalkDistance(), *request);
}

// Java SkillCooltimeResetAI.java:137-146
std::set<int32_t> SkillCooltimeResetAI::collectResettableItemCooldownIds(Player& player) {
	std::set<int32_t> itemCooldownIds;
	for (const auto& [delayId, cooldown] : player.getItemCoolDowns().snapshot())
		if (cooldown->getUseDelay() <= MAX_ITEM_COOLDOWN_SECONDS && cooldown->getReuseTime() > commons::utils::currentTimeMillis())
			itemCooldownIds.insert(delayId);
	if (!itemCooldownIds.empty()) {
		std::set<int32_t> buffItemAndPotionIds = collectBuffItemAndPotionCooldownIds(player);
		std::erase_if(itemCooldownIds, [&buffItemAndPotionIds](int32_t id) { return !buffItemAndPotionIds.contains(id); }); // retainAll
	}
	return itemCooldownIds;
}

// Java SkillCooltimeResetAI.java:148-165
std::set<int32_t> SkillCooltimeResetAI::collectBuffItemAndPotionCooldownIds(Player& player) {
	std::set<int32_t> cooldownIds;
	for (const runtime::Ptr<Item>& item : player.getInventory().getItems()) {
		const ItemTemplate* itemTemplate = item->getItemTemplate();
		const model::templates::item::ItemUseLimits* useLimits = itemTemplate->getUseLimits();
		if (useLimits == nullptr || useLimits->getDelayId() == 0)
			continue;
		if (itemTemplate->getActions() == nullptr || itemTemplate->getActions()->getSkillUseAction() == nullptr)
			continue;
		const SkillTemplate* skillTemplate = DataManager::SKILL_DATA->getSkillTemplate(itemTemplate->getActions()->getSkillUseAction()->getSkillId());
		if (skillTemplate == nullptr
			|| skillTemplate->getTargetSlot() != SkillTargetSlot::BUFF && !skillTemplate->getStack().starts_with("ITEM_POTION_")
				&& !skillTemplate->getStack().starts_with("ITEM_ARENA_POTION_"))
			continue;
		cooldownIds.insert(useLimits->getDelayId());
	}
	return cooldownIds;
}

// Java SkillCooltimeResetAI.java:167-180
std::set<int32_t> SkillCooltimeResetAI::collectResettableSkillCooldownIds(Player& player) {
	std::set<int32_t> cooldownIds;
	for (const runtime::Ptr<PlayerSkillEntry>& skill : player.getSkillList()->getAllSkills()) {
		const SkillTemplate* st = DataManager::SKILL_DATA->getSkillTemplate(skill->getSkillId());
		if (st == nullptr || st->getCooldown() > MAX_SKILL_COOLDOWN_TIME)
			continue;
		if (st->isDeityAvatar())
			continue;
		if (player.getSkillCoolDown(st->getCooldownId()) < commons::utils::currentTimeMillis())
			continue;
		cooldownIds.insert(st->getCooldownId());
	}
	return cooldownIds;
}

std::string SkillCooltimeResetAI::formatGrouped(int64_t value) {
	std::string digits = std::to_string(value < 0 ? -value : value);
	std::string grouped;
	for (size_t i = 0; i < digits.size(); i++) {
		if (i != 0 && (digits.size() - i) % 3 == 0)
			grouped += ',';
		grouped += digits[i];
	}
	return value < 0 ? "-" + grouped : grouped;
}

} // namespace aion::gameserver::handlers::ai
