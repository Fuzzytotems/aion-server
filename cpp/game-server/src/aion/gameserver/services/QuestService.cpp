#include "aion/gameserver/services/QuestService.h"

#include <algorithm>
#include <exception>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/configs/main/GroupConfig.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/ItemData.h"
#include "aion/gameserver/dataholders/QuestsData.h"
#include "aion/gameserver/dataholders/loadingutils/EnumTraits.h"
#include "aion/gameserver/model/Gender.h"
#include "aion/gameserver/model/PlayerClass.h"
#include "aion/gameserver/model/Race.h"
#include "aion/gameserver/model/drop/Drop.h"
#include "aion/gameserver/model/drop/DropItem.h"
#include "aion/gameserver/model/gameobjects/DropNpc.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/player/AbyssRank.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/QuestStateList.h"
#include "aion/gameserver/model/gameobjects/player/npcFaction/NpcFaction.h"
#include "aion/gameserver/model/gameobjects/player/npcFaction/NpcFactions.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/model/skill/PlayerSkillEntry.h"
#include "aion/gameserver/model/skill/PlayerSkillList.h"
#include "aion/gameserver/model/team/common/legacy/LootGroupRules.h"
#include "aion/gameserver/model/team/common/legacy/LootRuleType.h"
#include "aion/gameserver/model/team/group/PlayerGroup.h"
#include "aion/gameserver/model/templates/QuestTemplate.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/model/templates/quest/CollectItem.h"
#include "aion/gameserver/model/templates/quest/CollectItems.h"
#include "aion/gameserver/model/templates/quest/HandlerSideDrop.h"
#include "aion/gameserver/model/templates/quest/InventoryItem.h"
#include "aion/gameserver/model/templates/quest/InventoryItems.h"
#include "aion/gameserver/model/templates/quest/QuestCategory.h"
#include "aion/gameserver/model/templates/quest/QuestDrop.h"
#include "aion/gameserver/model/templates/quest/QuestMentorType.h"
#include "aion/gameserver/model/templates/quest/QuestTarget.h"
#include "aion/gameserver/model/templates/quest/XMLStartCondition.h"
#include "aion/gameserver/network/aion/serverpackets/SM_LOOT_STATUS.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/questEngine/model/QuestEnv.h"
#include "aion/gameserver/questEngine/model/QuestState.h"
#include "aion/gameserver/questEngine/model/QuestStatus.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/services/drop/DropRegistrationService.h"
#include "aion/gameserver/utils/ChatUtil.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/utils/PositionUtil.h"
#include "aion/gameserver/utils/stats/AbyssRankEnum.h"

namespace aion::gameserver::services {

namespace {

/** Java: DataManager.QUEST_DATA.getQuestById(questId) dereferenced right away (NullPointerException for an unknown quest) */
const model::templates::QuestTemplate* questTemplateOf(int32_t questId) {
	const model::templates::QuestTemplate* template_ = dataholders::DataManager::QUEST_DATA->getQuestById(questId);
	if (template_ == nullptr)
		throw runtime::NullPointerException("QUEST_DATA.getQuestById(" + std::to_string(questId) + ")");
	return template_;
}

/** Java: an Integer unboxed to int (NullPointerException for null) */
int32_t unboxed(const std::optional<int32_t>& value, const char* what) {
	if (!value)
		throw runtime::NullPointerException(what);
	return *value;
}

/**
 * Java: the `dropNpc` getQuestDrop reads from DropRegistrationService's map (QuestService.java:671) and hands to allowLooting, which calls
 * dropNpc.setAllowedLooter (:739). registerDrop registers the npc's DropNpc (initDropNpc, DropRegistrationService.java:68) before it calls
 * getQuestDrop (:84), so null needs another caller; Java's NullPointerException is thrown here, before allowLooting's first statement.
 */
model::gameobjects::DropNpc& droppingNpc(const runtime::Ptr<model::gameobjects::DropNpc>& dropNpc) {
	if (!dropNpc)
		throw runtime::NullPointerException("dropNpc");
	return *dropNpc;
}

/**
 * Java: `drop instanceof HandlerSideDrop handlerSideDrop` (QuestService.java:781). QuestDrop is a static template without a virtual member, so
 * C++ cannot dynamic_cast it (header request m5b3-loot-h01 asks for a virtual destructor). The two sources of QuestService.questDrop decide it
 * instead: QuestEngine::init adds every <quest_drop> element of the quest templates, i.e. pointers into QuestTemplate::getQuestDrop() of the
 * drop's own quest (QuestsData sets the drop's questId from that template), and QuestEngine::addHandlerSideQuestDrop adds the HandlerSideDrops
 * it allocates. A drop outside its quest template's list is therefore a HandlerSideDrop.
 *
 * @param qt the template of `drop->getQuestId()`
 */
const model::templates::quest::HandlerSideDrop* asHandlerSideDrop(const model::templates::QuestTemplate& qt,
	const model::templates::quest::QuestDrop* drop) {
	const std::vector<model::templates::quest::QuestDrop>& xmlDrops = qt.getQuestDrop();
	std::less<const model::templates::quest::QuestDrop*> before;
	if (!xmlDrops.empty() && !before(drop, xmlDrops.data()) && before(drop, xmlDrops.data() + xmlDrops.size()))
		return nullptr;
	return static_cast<const model::templates::quest::HandlerSideDrop*>(drop);
}

/** Java: AbyssRankEnum.getRankL10n(race, rankId): getRankById(rankId).getRankL10n(race) */
std::string rankL10nOf(model::Race race, int32_t rankId) {
	const auto& ranks = xml::EnumTraits<utils::stats::AbyssRankEnum>::names;
	if (rankId < 1 || rankId > static_cast<int32_t>(ranks.size())) // Java getRankById: getId() is ordinal + 1
		throw runtime::IllegalArgumentException("Invalid abyss rank provided " + std::to_string(rankId));
	int32_t rank9L10nId = race == model::Race::ELYOS ? 901215 : 901233;
	return utils::ChatUtil::l10n(rank9L10nId + (rankId - 1));
}

} // namespace

// Anonymous classes and stored lambdas of the Java class (hub-headers.md §7.3): the bodies that create them define the structs that
// `python tools/gen/fieldmap.py --class <key>` prints.
//   anonymous Runnable at QuestService.java:815 (com.aionemu.gameserver.services.QuestService$1); argument 1 of schedule(); storage: task
//   anonymous Runnable at QuestService.java:831 (com.aionemu.gameserver.services.QuestService$2); argument 1 of schedule(); storage: task

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.services.QuestService");

runtime::HashMap<int32_t, runtime::Ref<runtime::RcArrayList<const model::templates::quest::QuestDrop*>>> QuestService::questDrop{
	AION_LOCK_CLASS(QuestService::questDrop)};

bool QuestService::finishQuest(questEngine::model::QuestEnv& env) {
	AION_UNPORTED();
}

void QuestService::validateAndFixRewardGroup(runtime::Ptr<questEngine::model::QuestState> qs, int32_t questId) {
	AION_UNPORTED();
}

std::vector<const model::templates::quest::QuestItems*> QuestService::getRewardItems(questEngine::model::QuestEnv& env,
	const model::templates::QuestTemplate* template_, bool extended, std::optional<int32_t> rewardGroup) {
	AION_UNPORTED();
}

int32_t QuestService::getRewardIndex(int32_t dialogActionId) {
	AION_UNPORTED();
}

void QuestService::giveReward(questEngine::model::QuestEnv& env, const model::templates::quest::Rewards* rewards) {
	AION_UNPORTED();
}

std::optional<commons::database::Timestamp> QuestService::calculateRepeatDate(model::gameobjects::player::Player& player,
	const model::templates::QuestTemplate* template_) {
	AION_UNPORTED();
}

model::templates::quest::QuestRepeatCycle QuestService::findNextRepeatDay(
	const std::vector<model::templates::quest::QuestRepeatCycle>& questRepeatDays, std::chrono::weekday day) {
	AION_UNPORTED();
}

bool QuestService::checkStartConditions(model::gameobjects::player::Player& player, int32_t questId, bool warn) {
	return checkStartConditions(player, questId, warn, 0, false, false, false);
}

bool QuestService::checkStartConditions(model::gameobjects::player::Player& player, int32_t questId, bool warn, int32_t allowedDiffToMinLevel,
	bool skipStartedCheck, bool skipRepeatCountCheck, bool skipXmlPreconditionCheck) {
	using network::aion::serverpackets::SM_SYSTEM_MESSAGE;
	using questEngine::model::QuestStatus;
	try {
		runtime::Ptr<questEngine::model::QuestState> qs = player.getQuestStateList()->getQuestState(questId);
		if (qs) {
			if (!skipStartedCheck && (qs->getStatus() == QuestStatus::START || qs->getStatus() == QuestStatus::REWARD)) {
				if (warn)
					utils::PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_QUEST_ACQUIRE_ERROR_WORKING_QUEST());
				return false;
			} else if (!skipRepeatCountCheck && qs->getStatus() == QuestStatus::COMPLETE && !qs->canRepeat()) {
				const model::templates::QuestTemplate* template_ = questTemplateOf(questId);
				if (template_->getMaxRepeatCount() > 1 && template_->getMaxRepeatCount() != 255 && qs->getCompleteCount() >= template_->getMaxRepeatCount()) {
					if (warn)
						utils::PacketSendUtility::sendPacket(player,
							SM_SYSTEM_MESSAGE::STR_QUEST_ACQUIRE_ERROR_MAX_REPEAT_COUNT(utils::ChatUtil::quest(questId), template_->getMaxRepeatCount()));
				} else {
					if (warn)
						utils::PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_QUEST_ACQUIRE_ERROR_NONE_REPEATABLE(utils::ChatUtil::quest(questId)));
				}
				return false;
			}
		}

		const model::templates::QuestTemplate* template_ = questTemplateOf(questId);
		if (template_->getRacePermitted() && *template_->getRacePermitted() != model::Race::PC_ALL && *template_->getRacePermitted() != player.getRace()) {
			if (warn)
				utils::PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_QUEST_ACQUIRE_ERROR_RACE());
			return false;
		}

		// min level - 2 so that the gray quest arrow shows when quest is almost available
		int32_t levelDiff = template_->getMinlevelPermitted() - allowedDiffToMinLevel - player.getLevel();
		if (levelDiff > 0) {
			if (warn)
				utils::PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_QUEST_ACQUIRE_ERROR_MIN_LEVEL(template_->getMinlevelPermitted()));
			return false;
		}

		if (template_->getMaxlevelPermitted() != 0 && player.getLevel() > template_->getMaxlevelPermitted()) {
			if (warn)
				utils::PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_QUEST_ACQUIRE_ERROR_MAX_LEVEL(template_->getMaxlevelPermitted()));
			return false;
		}

		const std::vector<model::PlayerClass>& classPermitted = template_->getClassPermitted();
		if (!classPermitted.empty() && std::ranges::find(classPermitted, player.getPlayerClass()) == classPermitted.end()) {
			if (warn)
				utils::PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_QUEST_ACQUIRE_ERROR_CLASS());
			return false;
		}

		if (template_->getGenderPermitted() && *template_->getGenderPermitted() != player.getGender()) {
			if (warn)
				utils::PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_QUEST_ACQUIRE_ERROR_GENDER());
			return false;
		}

		// Java: player.getAbyssRank().getRank().getId() (ordinal + 1)
		if (template_->getRequiredRank() != 0 && static_cast<int32_t>(player.getAbyssRank()->getRank()) + 1 < template_->getRequiredRank()) {
			if (warn)
				utils::PacketSendUtility::sendPacket(player,
					SM_SYSTEM_MESSAGE::STR_QUEST_ACQUIRE_ERROR_MIN_RANK(rankL10nOf(player.getRace(), template_->getRequiredRank())));
			return false;
		}

		if (!skipXmlPreconditionCheck) {
			int32_t fulfilledStartConditions = 0;
			for (const model::templates::quest::XMLStartCondition& startCondition : template_->getXMLStartConditions()) {
				if (startCondition.check(player, warn))
					fulfilledStartConditions++;
			}
			if (fulfilledStartConditions < template_->getRequiredConditionCount())
				return false;
		}

		runtime::Ref<questEngine::model::QuestEnv> env = questEngine::model::QuestEnv::create(nullptr, player, questId);
		if (!inventoryItemCheck(*env, warn))
			return false;

		if (!checkCombineSkill(*env, warn))
			return false;

		// check if NpcFaction daily quest
		if (template_->getNpcFactionId() != 0) {
			// check if the NpcFaction daily time limit has passed
			if (!template_->isTimeBased() && !player.getNpcFactions().canStartQuest(template_))
				return false;

			runtime::Ptr<model::gameobjects::player::npcFaction::NpcFaction> faction = player.getNpcFactions().getFactionById(template_->getNpcFactionId());
			if (!faction || !faction->isActive())
				return false;
		}

		return true;
	} catch (const std::exception& ex) {
		log.error("QE: exception in checkStartCondition (" + player.toString() + ", questId " + std::to_string(questId) + ")", ex);
	}
	return false;
}

bool QuestService::startQuest(questEngine::model::QuestEnv& env) {
	AION_UNPORTED();
}

bool QuestService::startQuest(questEngine::model::QuestEnv& env, questEngine::model::QuestStatus status, bool warn) {
	AION_UNPORTED();
}

void QuestService::addOrUpdateQuest(model::gameobjects::player::Player& player, int32_t questId, questEngine::model::QuestStatus status) {
	AION_UNPORTED();
}

bool QuestService::checkCombineSkill(questEngine::model::QuestEnv& env, bool warn) {
	runtime::Ptr<model::gameobjects::player::Player> player = env.getPlayer();
	const model::templates::QuestTemplate* template_ = dataholders::DataManager::QUEST_DATA->getQuestById(env.getQuestId());

	if (template_ == nullptr)
		return false;

	if (template_->getCombineSkill() != 0) {
		std::vector<int32_t> skills; // skills to check
		if (template_->getCombineSkill() == -1) { // any skill
			if (template_->getNpcFactionId() != 12 && template_->getNpcFactionId() != 13) { // exclude essence/aether tapping for crafting dailies
				skills.push_back(30002);
				skills.push_back(30003);
			}
			skills.push_back(40001);
			skills.push_back(40002);
			skills.push_back(40003);
			skills.push_back(40004);
			skills.push_back(40007);
			skills.push_back(40008);
			skills.push_back(40010);
		} else {
			skills.push_back(template_->getCombineSkill());
		}
		bool result = false;
		for (int32_t skillId : skills) {
			runtime::Ptr<model::skill::PlayerSkillEntry> skill = player->getSkillList()->getSkillEntry(skillId);
			if (skill && skill->getSkillLevel() >= template_->getCombineSkillPoint()) {
				if (template_->getCategory() == model::templates::quest::QuestCategory::TASK && skill->getSkillLevel() - 40 > template_->getCombineSkillPoint())
					continue;
				result = true;
				break;
			}
		}
		if (!result) {
			if (warn)
				utils::PacketSendUtility::sendPacket(*player, network::aion::serverpackets::SM_SYSTEM_MESSAGE::STR_QUEST_ACQUIRE_ERROR_TS_RANK(
					std::to_string(template_->getCombineSkillPoint())));
			return false;
		}
	}

	return true;
}

bool QuestService::startEventQuest(questEngine::model::QuestEnv& env, questEngine::model::QuestStatus questStatus) {
	AION_UNPORTED();
}

bool QuestService::checkQuestListSize(model::gameobjects::player::QuestStateList& qsl) {
	AION_UNPORTED();
}

bool QuestService::collectItemCheck(questEngine::model::QuestEnv& env, bool removeItem) {
	AION_UNPORTED();
}

bool QuestService::inventoryItemCheck(questEngine::model::QuestEnv& env, bool showWarning) {
	runtime::Ptr<model::gameobjects::player::Player> player = env.getPlayer();
	const model::templates::QuestTemplate* template_ = questTemplateOf(env.getQuestId());
	const model::templates::quest::InventoryItems* inventoryItems = template_->getInventoryItems();
	if (inventoryItems != nullptr) {
		// Usually counts are 1, and if more, then collect item checks exist
		// Other quests having no collect item checks and counts greater than 1 are unused (old coin exchange quests)
		for (const model::templates::quest::InventoryItem& inventoryItem : inventoryItems->getInventoryItems()) {
			int32_t itemId = unboxed(inventoryItem.getItemId(), "inventoryItem.getItemId()");
			if (!player->getInventory().getFirstItemByItemId(itemId)) {
				if (showWarning) {
					const model::templates::item::ItemTemplate* itemTemplate = dataholders::DataManager::ITEM_DATA->getItemTemplate(itemId);
					if (itemTemplate == nullptr)
						throw runtime::NullPointerException("ITEM_DATA.getItemTemplate(" + std::to_string(itemId) + ")");
					std::string requiredItemL10n = itemTemplate->getL10n();
					utils::PacketSendUtility::sendPacket(*player,
						network::aion::serverpackets::SM_SYSTEM_MESSAGE::STR_QUEST_ACQUIRE_ERROR_INVENTORY_ITEM(requiredItemL10n));
				}
				return false;
			}
		}
	}
	return true;
}

int32_t QuestService::checkAndGetCollectItemQuestRewardCategory(questEngine::model::QuestEnv& env) {
	AION_UNPORTED();
}

int32_t QuestService::checkAndGetCollectItemQuestRewardCategory(questEngine::model::QuestEnv& env, std::optional<int32_t> rewardIndex) {
	AION_UNPORTED();
}

int32_t QuestService::getQuestDrop(runtime::RcHashSet<runtime::Ref<model::drop::DropItem>>& dropItems, int32_t index,
	model::gameobjects::Npc& npc, const std::vector<runtime::Ptr<model::gameobjects::player::Player>>& players,
	model::gameobjects::player::Player& player) {
	using model::gameobjects::player::Player;
	std::vector<const model::templates::quest::QuestDrop*> drops = getQuestDrop(npc.getNpcId());
	if (drops.empty()) {
		return index;
	}
	runtime::Ptr<model::gameobjects::DropNpc> dropNpc = drop::DropRegistrationService::getInstance().getDropRegistrationMap().get(npc.getObjectId());
	for (const model::templates::quest::QuestDrop* drop : drops) {
		if (commons::utils::Rnd::chance() >= static_cast<float>(drop->getChance()))
			continue;

		// an empty `players` stands for Java null (m5b3-plan.md D11): NpcController.doReward passes null for a solo kill
		if (!players.empty() && player.isInGroup()) {
			std::vector<runtime::Ptr<Player>> pls;
			if (drop->isDropEachMemberGroup()) {
				for (const runtime::Ptr<Player>& member : players) {
					if (isQuestDrop(*member, drop)) {
						pls.push_back(member);
						dropItems.add(regQuestDropItem(drop, index++, member->getObjectId()));
					}
				}
			} else {
				for (const runtime::Ptr<Player>& member : players) {
					if (isQuestDrop(*member, drop)) {
						pls.push_back(member);
						break;
					}
				}
			}
			if (pls.size() > 0) {
				runtime::Ptr<model::drop::DropItem> dItem = nullptr;
				if (!drop->isDropEachMemberGroup()) {
					runtime::Ref<model::drop::DropItem> created = regQuestDropItem(drop, index++, 0);
					dItem = created;
					dropItems.add(created);
				}
				allowLooting(pls, droppingNpc(dropNpc), dItem);
			}
		} else if (!players.empty() && player.isInAlliance()) {
			std::vector<runtime::Ptr<Player>> pls;
			if (drop->isDropEachMemberAlliance()) {
				for (const runtime::Ptr<Player>& member : players) {
					if (isQuestDrop(*member, drop)) {
						pls.push_back(member);
						dropItems.add(regQuestDropItem(drop, index++, member->getObjectId()));
					}
				}
			} else {
				for (const runtime::Ptr<Player>& member : players) {
					if (isQuestDrop(*member, drop)) {
						pls.push_back(member);
						break;
					}
				}
			}
			if (pls.size() > 0) {
				runtime::Ptr<model::drop::DropItem> dItem = nullptr;
				if (!drop->isDropEachMemberAlliance()) {
					runtime::Ref<model::drop::DropItem> created = regQuestDropItem(drop, index++, 0);
					dItem = created;
					dropItems.add(created);
				}
				allowLooting(pls, droppingNpc(dropNpc), dItem);
			}
		} else {
			if (isQuestDrop(player, drop)) {
				dropItems.add(regQuestDropItem(drop, index++, player.getObjectId()));
			}
		}
	}
	return index;
}

void QuestService::allowLooting(const std::vector<runtime::Ptr<model::gameobjects::player::Player>>& players, model::gameobjects::DropNpc& dropNpc,
	runtime::Ptr<model::drop::DropItem> dropItem) {
	for (const runtime::Ptr<model::gameobjects::player::Player>& player : players) {
		if (dropItem)
			dropItem->setPlayerObjId(player->getObjectId());
		dropNpc.setAllowedLooter(*player);
		if (dropNpc.getLootGroupRules() && dropNpc.getLootGroupRules()->getLootRule() != model::team::common::legacy::LootRuleType::FREEFORALL) {
			utils::PacketSendUtility::sendPacket(*player, network::aion::serverpackets::SM_LOOT_STATUS(dropNpc.getObjectId(),
															  network::aion::serverpackets::SM_LOOT_STATUS::Status::LOOT_ENABLE));
		}
	}
}

runtime::Ref<model::drop::DropItem> QuestService::regQuestDropItem(const model::templates::quest::QuestDrop* drop, int32_t index,
	std::optional<int32_t> winner) {
	runtime::Ref<model::drop::DropItem> item = model::drop::DropItem::create(
		model::drop::Drop(unboxed(drop->getItemId(), "drop.getItemId()"), 1, 1, static_cast<float>(drop->getChance())));
	item->setPlayerObjId(unboxed(winner, "winner"));
	item->setIndex(index);
	item->setCount(1);
	return item;
}

bool QuestService::isQuestDrop(model::gameobjects::player::Player& player, const model::templates::quest::QuestDrop* drop) {
	int32_t questId = unboxed(drop->getQuestId(), "drop.getQuestId()");
	runtime::Ptr<questEngine::model::QuestState> qs = player.getQuestStateList()->getQuestState(questId);
	if (!qs || qs->getStatus() != questEngine::model::QuestStatus::START) {
		return false;
	}
	if (drop->getCollectingStep() != 0) {
		// QuestState::getQuestVarById is P5-06's and unported (QuestState.cpp, QuestVars::getVarById): reached only for a started quest whose
		// drop has a collecting step, which needs a quest that can start - M5d (m5b3-plan.md L-04, O-06)
		if (drop->getCollectingStep() != qs->getQuestVarById(0)) {
			return false;
		}
	}
	const model::templates::QuestTemplate* qt = questTemplateOf(questId);
	if (qt->getTarget() == model::templates::quest::QuestTarget::ALLIANCE) {
		if (!player.isInAlliance()) {
			return false;
		}
	}
	if (qt->getMentorType() == model::templates::quest::QuestMentorType::MENTE) {
		if (!player.isInGroup()) {
			return false;
		}

		runtime::Ptr<model::team::group::PlayerGroup> group = player.getPlayerGroup();
		bool noneMatch = true; // Java: group.getMembers().stream().noneMatch(member -> member.isMentor() && isInRange(...))
		for (const runtime::Ptr<model::gameobjects::AionObject>& object : group->getMembers()) {
			runtime::Ptr<model::gameobjects::player::Player> member = runtime::cast<model::gameobjects::player::Player>(object);
			if (member->isMentor() &&
				utils::PositionUtil::isInRange(player, *member, static_cast<float>(configs::main::GroupConfig::GROUP_MAX_DISTANCE.load()))) {
				noneMatch = false;
				break;
			}
		}
		if (noneMatch) {
			return false;
		}
	}
	if (const model::templates::quest::HandlerSideDrop* handlerSideDrop = asHandlerSideDrop(*qt, drop)) {
		return handlerSideDrop->getNeededAmount() > player.getInventory().getItemCountByItemId(unboxed(drop->getItemId(), "drop.getItemId()"));
	}

	const model::templates::quest::CollectItems* collectItems = questTemplateOf(questId)->getCollectItems();
	if (collectItems == nullptr)
		return true;

	for (const model::templates::quest::CollectItem& collectItem : collectItems->getCollectItem()) {
		int32_t collectItemId = unboxed(collectItem.getItemId(), "collectItem.getItemId()");
		int64_t count = player.getInventory().getItemCountByItemId(collectItemId);
		if (unboxed(collectItem.getCount(), "collectItem.getCount()") > count && unboxed(drop->getItemId(), "drop.getItemId()") == collectItemId)
			return true;
	}
	return false;
}

bool QuestService::checkLevelRequirement(int32_t questId, int32_t playerLevel) {
	return checkLevelRequirement(dataholders::DataManager::QUEST_DATA->getQuestById(questId), playerLevel);
}

bool QuestService::checkLevelRequirement(const model::templates::QuestTemplate* qt, int32_t playerLevel) {
	if (qt == nullptr)
		throw runtime::NullPointerException("qt");
	return playerLevel >= qt->getMinlevelPermitted() && (qt->getMaxlevelPermitted() == 0 || playerLevel <= qt->getMaxlevelPermitted());
}

int32_t QuestService::getLevelRequirementDiff(int32_t questId, int32_t playerLevel) {
	const model::templates::QuestTemplate* template_ = dataholders::DataManager::QUEST_DATA->getQuestById(questId);
	return template_ == nullptr ? 99 : template_->getMinlevelPermitted() - playerLevel;
}

bool QuestService::questTimerStart(questEngine::model::QuestEnv& env, int32_t timeInSeconds) {
	AION_UNPORTED();
}

bool QuestService::invisibleTimerStart(questEngine::model::QuestEnv& env, int32_t timeInSeconds) {
	AION_UNPORTED();
}

bool QuestService::questTimerEnd(questEngine::model::QuestEnv& env) {
	AION_UNPORTED();
}

bool QuestService::abandonQuest(model::gameobjects::player::Player& player, int32_t questId) {
	AION_UNPORTED();
}

std::vector<const model::templates::quest::QuestDrop*> QuestService::getQuestDrop(int32_t npcId) {
	// Java: questDrop.getOrDefault(npcId, Collections.emptyList()) (the live list; C++: a snapshot)
	runtime::Ptr<runtime::RcArrayList<const model::templates::quest::QuestDrop*>> drops = questDrop.get(npcId);
	return drops ? drops->snapshot() : std::vector<const model::templates::quest::QuestDrop*>();
}

void QuestService::addQuestDrop(int32_t npcId, const model::templates::quest::QuestDrop* drop) {
	runtime::Ptr<runtime::RcArrayList<const model::templates::quest::QuestDrop*>> drops =
		questDrop.computeIfAbsent(npcId, [] { return runtime::RcArrayList<const model::templates::quest::QuestDrop*>::create(); });
	drops->add(drop);
}

void QuestService::clearQuestDrops() {
	questDrop.clear();
}

std::vector<runtime::Ptr<model::gameobjects::player::Player>> QuestService::getEachDropMembersGroup(model::team::group::PlayerGroup& group,
	int32_t npcId, int32_t questId) {
	AION_UNPORTED();
}

std::vector<runtime::Ptr<model::gameobjects::player::Player>> QuestService::getEachDropMembersAlliance(
	model::team::alliance::PlayerAlliance& alliance, int32_t npcId, int32_t questId) {
	AION_UNPORTED();
}

void QuestService::removeQuestWorkItems(model::gameobjects::player::Player& player, questEngine::model::QuestState& qs) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::services
