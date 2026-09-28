#include "aion/gameserver/services/QuestService.h"

#include <algorithm>
#include <chrono>
#include <exception>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/configs/main/CustomConfig.h"
#include "aion/gameserver/configs/main/GroupConfig.h"
#include "aion/gameserver/configs/main/MembershipConfig.h"
#include "aion/gameserver/controllers/PlayerController.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/ItemData.h"
#include "aion/gameserver/dataholders/NpcData.h"
#include "aion/gameserver/dataholders/QuestsData.h"
#include "aion/gameserver/dataholders/XMLQuests.h"
#include "aion/gameserver/dataholders/loadingutils/EnumTraits.h"
#include "aion/gameserver/model/DialogAction.h"
#include "aion/gameserver/model/Gender.h"
#include "aion/gameserver/model/PlayerClass.h"
#include "aion/gameserver/model/Race.h"
#include "aion/gameserver/model/TaskId.h"
#include "aion/gameserver/model/drop/Drop.h"
#include "aion/gameserver/model/drop/DropItem.h"
#include "aion/gameserver/model/gameobjects/DropNpc.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/player/AbyssRank.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"
#include "aion/gameserver/model/gameobjects/player/QuestStateList.h"
#include "aion/gameserver/model/gameobjects/player/RatesInfo.h"
#include "aion/gameserver/model/gameobjects/player/RecipeList.h"
#include "aion/gameserver/model/gameobjects/player/npcFaction/NpcFaction.h"
#include "aion/gameserver/model/gameobjects/player/npcFaction/NpcFactions.h"
#include "aion/gameserver/model/gameobjects/player/title/TitleList.h"
#include "aion/gameserver/model/items/ItemId.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/model/skill/PlayerSkillEntry.h"
#include "aion/gameserver/model/skill/PlayerSkillList.h"
#include "aion/gameserver/model/team/alliance/PlayerAlliance.h"
#include "aion/gameserver/model/team/common/legacy/LootGroupRules.h"
#include "aion/gameserver/model/team/common/legacy/LootRuleType.h"
#include "aion/gameserver/model/team/group/PlayerGroup.h"
#include "aion/gameserver/model/templates/QuestTemplate.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/model/templates/npc/NpcTemplate.h"
#include "aion/gameserver/model/templates/quest/CollectItem.h"
#include "aion/gameserver/model/templates/quest/CollectItems.h"
#include "aion/gameserver/model/templates/quest/HandlerSideDrop.h"
#include "aion/gameserver/model/templates/quest/InventoryItem.h"
#include "aion/gameserver/model/templates/quest/InventoryItems.h"
#include "aion/gameserver/model/templates/quest/QuestBonuses.h"
#include "aion/gameserver/model/templates/quest/QuestCategory.h"
#include "aion/gameserver/model/templates/quest/QuestDrop.h"
#include "aion/gameserver/model/templates/quest/QuestItems.h"
#include "aion/gameserver/model/templates/quest/QuestMentorType.h"
#include "aion/gameserver/model/templates/quest/QuestRepeatCycleInfo.h"
#include "aion/gameserver/model/templates/quest/QuestTarget.h"
#include "aion/gameserver/model/templates/quest/QuestWorkItems.h"
#include "aion/gameserver/model/templates/quest/Rewards.h"
#include "aion/gameserver/model/templates/quest/XMLStartCondition.h"
#include "aion/gameserver/network/aion/serverpackets/SM_LOOT_STATUS.h"
#include "aion/gameserver/network/aion/serverpackets/SM_QUEST_ACTION.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/questEngine/QuestEngine.h"
#include "aion/gameserver/questEngine/handlers/HandlerResult.h"
#include "aion/gameserver/questEngine/handlers/models/WorkOrdersData.h"
#include "aion/gameserver/questEngine/handlers/models/XMLQuest.h"
#include "aion/gameserver/questEngine/model/QuestEnv.h"
#include "aion/gameserver/questEngine/model/QuestState.h"
#include "aion/gameserver/questEngine/model/QuestStatus.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/sched/Clock.h"
#include "aion/gameserver/services/ChallengeTaskService.h"
#include "aion/gameserver/services/CubeExpandService.h"
#include "aion/gameserver/services/WarehouseService.h"
#include "aion/gameserver/services/abyss/AbyssPointsService.h"
#include "aion/gameserver/services/abyss/GloryPointsService.h"
#include "aion/gameserver/services/drop/DropRegistrationService.h"
#include "aion/gameserver/services/item/ItemPacketService_ItemUpdateType.h"
#include "aion/gameserver/services/item/ItemService.h"
#include "aion/gameserver/services/reward/BonusService.h"
#include "aion/gameserver/utils/ChatUtil.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/utils/PositionUtil.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"
#include "aion/gameserver/utils/audit/AuditLogger.h"
#include "aion/gameserver/utils/stats/AbyssRankEnum.h"
#include "aion/gameserver/utils/time/ServerTime.h"

namespace aion::gameserver::services {

namespace {

using network::aion::serverpackets::SM_QUEST_ACTION;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;
using questEngine::model::QuestStatus;

/** Java int arithmetic (two's complement wrap-around): client and data values reach these sums unchecked */
constexpr int32_t javaSub(int32_t a, int32_t b) noexcept {
	return static_cast<int32_t>(static_cast<uint32_t>(a) - static_cast<uint32_t>(b));
}

constexpr int32_t javaAdd(int32_t a, int32_t b) noexcept {
	return static_cast<int32_t>(static_cast<uint32_t>(a) + static_cast<uint32_t>(b));
}

constexpr int32_t javaMul(int32_t a, int32_t b) noexcept {
	return static_cast<int32_t>(static_cast<uint32_t>(a) * static_cast<uint32_t>(b));
}

/** Java List.get(index): IndexOutOfBoundsException (JDK wording) outside [0, size) */
template <class T>
const T& listGet(const std::vector<T>& list, int32_t index) {
	if (index < 0 || static_cast<size_t>(index) >= list.size())
		throw runtime::IndexOutOfBoundsException("Index " + std::to_string(index) + " out of bounds for length " + std::to_string(list.size()));
	return list[static_cast<size_t>(index)];
}

/** Java List.addAll: the source's elements appended in order */
void addAll(std::vector<model::templates::quest::QuestItems>& list, const std::vector<model::templates::quest::QuestItems>& items) {
	list.insert(list.end(), items.begin(), items.end());
}

/**
 * Java System.currentTimeMillis() behind ServerTime.now() (QuestService.java:247). C++: the clock of the installed scheduler backend - the
 * system clock in the server, the ManualClock under a DeterministicExecutor - the time line QuestState reads in canRepeat
 * (docs/deviations/P5-06a.md).
 */
int64_t currentTimeMillis() {
	return utils::ThreadPoolManager::clock().currentTimeMillis();
}

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
	using model::templates::quest::QuestCategory;
	using model::templates::quest::QuestItems;
	using model::templates::quest::Rewards;
	runtime::Ptr<model::gameobjects::player::Player> player = env.getPlayer();
	int32_t id = env.getQuestId();
	runtime::Ptr<questEngine::model::QuestState> qs = player->getQuestStateList()->getQuestState(id);

	const Rewards newRewards; // Java: new Rewards() - every amount 0, no items
	const Rewards newExtendedRewards;
	const Rewards* rewards = &newRewards;
	const Rewards* extendedRewards = &newExtendedRewards;
	if (!qs || qs->getStatus() != QuestStatus::REWARD)
		return false;
	const model::templates::QuestTemplate* template_ = questTemplateOf(id);
	if (template_->getCategory() == QuestCategory::MISSION && qs->getCompleteCount() != 0)
		return false; // prevent repeatable reward because of wrong quest handling

	validateAndFixRewardGroup(qs, id);
	std::vector<QuestItems> questItems;
	if (template_->getExtendedRewards() != nullptr && qs->getCompleteCount() == javaSub(template_->getRewardRepeatCount(), 1)) { // additional reward for the Xth time
		addAll(questItems, getRewardItems(env, template_, true, std::nullopt));
		extendedRewards = template_->getExtendedRewards();
	}
	if (!template_->getRewards().empty() || template_->getBonus() != nullptr) {
		addAll(questItems, getRewardItems(env, template_, false, qs->getRewardGroup()));
		if (qs->getRewardGroup())
			rewards = &listGet(template_->getRewards(), *qs->getRewardGroup());
	}
	for (const QuestItems& qi : questItems)
		item::ItemService::addItem(*player, qi.getItemId(), qi.getCount(), true);
	giveReward(env, rewards);
	giveReward(env, extendedRewards);
	if (template_->getCategory() == QuestCategory::CHALLENGE_TASK)
		ChallengeTaskService::getInstance().onChallengeQuestFinish(*player, id);
	removeQuestWorkItems(*player, *qs); // remove all worker list item if finished
	qs->setStatus(QuestStatus::COMPLETE);
	qs->setQuestVar(0);
	if (template_->isTimeBased())
		qs->setNextRepeatTime(calculateRepeatDate(*player, template_));
	utils::PacketSendUtility::sendPacket(*player, SM_QUEST_ACTION(SM_QUEST_ACTION::ActionType::UPDATE, *qs));
	questEngine::QuestEngine::getInstance().onQuestCompleted(*player, id);
	if (template_->getNpcFactionId() != 0)
		player->getNpcFactions().completeQuest(template_);
	player->getController().updateNearbyQuests();
	return true;
}

void QuestService::validateAndFixRewardGroup(runtime::Ptr<questEngine::model::QuestState> qs, int32_t questId) {
	if (qs && qs->getStatus() == QuestStatus::REWARD) {
		// Java getRewards() answers Collections.emptyList() without <rewards>, never null (QuestTemplate.java:146-148): the `rewardGroups == null`
		// arm of QuestService.java:127-129 cannot run, and an empty list takes the nonexistent-group arm, as in Java
		const std::vector<model::templates::quest::Rewards>& rewardGroups = questTemplateOf(questId)->getRewards();
		if (qs->getRewardGroup()) {
			int32_t rewardGroup = *qs->getRewardGroup();
			if (rewardGroup < 0 || rewardGroup >= static_cast<int32_t>(rewardGroups.size())) {
				log.warn("Handler for quest " + std::to_string(questId) + " tried to reward a nonexistent reward group (index " +
						 std::to_string(rewardGroup) + ").");
				qs->setRewardGroup(static_cast<int32_t>(rewardGroups.size()) - 1);
			}
		} else { // you must explicitly specify the reward group when there are more than 1
			if (rewardGroups.size() > 0) {
				if (rewardGroups.size() > 1)
					log.warn("Handler for quest " + std::to_string(questId) + " possibly rewarded the wrong reward group.");
				qs->setRewardGroup(0);
			}
		}
	}
}

std::vector<model::templates::quest::QuestItems> QuestService::getRewardItems(questEngine::model::QuestEnv& env,
	const model::templates::QuestTemplate* template_, bool extended, std::optional<int32_t> rewardGroup) {
	using model::DialogAction::SELECTED_QUEST_NOREWARD;
	using model::templates::quest::QuestItems;
	using model::templates::quest::Rewards;
	runtime::Ptr<model::gameobjects::player::Player> player = env.getPlayer();
	int32_t id = env.getQuestId();
	int32_t dialogActionId = env.getDialogActionId();
	std::vector<QuestItems> questItems;
	if (extended) {
		const Rewards* rewards = template_->getExtendedRewards();
		if (rewards == nullptr) // Java: rewards.getRewardItem() on a null template.getExtendedRewards()
			throw runtime::NullPointerException("template.getExtendedRewards()");
		addAll(questItems, rewards->getRewardItem());
		if (dialogActionId == SELECTED_QUEST_NOREWARD && !rewards->getSelectableRewardItem().empty()) {
			const std::vector<QuestItems>& selectable = rewards->getSelectableRewardItem();
			const int32_t size = static_cast<int32_t>(selectable.size());
			int32_t index = env.getExtendedRewardIndex();
			if (javaSub(index, 8) >= 0 && javaSub(index, 8) < size) {
				questItems.push_back(selectable[static_cast<size_t>(javaSub(index, 8))]);
			} else if (javaSub(index, 1) >= 0 && javaSub(index, 1) < size) {
				questItems.push_back(selectable[static_cast<size_t>(javaSub(index, 1))]);
			} else {
				log.warn("The extended SelectableRewardItem list has no element on index " + std::to_string(javaSub(index, 8)) + ". See quest id " +
						 std::to_string(env.getQuestId()) + ". The size is: " + std::to_string(size));
			}
		}
	} else {
		if (rewardGroup) {
			const Rewards& rewards = listGet(template_->getRewards(), *rewardGroup);
			addAll(questItems, rewards.getRewardItem());
			runtime::Ptr<questEngine::model::QuestState> qs = player->getQuestStateList()->getQuestState(id);
			model::PlayerClass playerClass = player->getCommonData()->getPlayerClass();
			int32_t rewardIndex = getRewardIndex(env.getDialogActionId());
			if (rewardIndex >= 0) {
				bool isLastRepeat = qs->getCompleteCount() == javaSub(template_->getRewardRepeatCount(), 1);
				if ((isLastRepeat && template_->isSingleTimeClassReward()) || template_->isClassRewardOnEveryRepeat()) {
					const std::vector<QuestItems>& byClass = template_->getSelectableRewardByClass(playerClass);
					if (rewardIndex < static_cast<int32_t>(byClass.size())) {
						questItems.push_back(byClass[static_cast<size_t>(rewardIndex)]);
					} else {
						log.warn("The SelectableRewardByClass list has no element on index " + std::to_string(rewardIndex) + ". See quest id " +
								 std::to_string(env.getQuestId()) + ". The size for " + std::string(xml::enumName(playerClass)) + " is: " +
								 std::to_string(byClass.size()));
					}
				} else if (rewardIndex < static_cast<int32_t>(rewards.getSelectableRewardItem().size())) {
					questItems.push_back(rewards.getSelectableRewardItem()[static_cast<size_t>(rewardIndex)]);
				} else {
					log.warn("The SelectableRewardItem list has no element on index " + std::to_string(rewardIndex) + ". See quest id " +
							 std::to_string(env.getQuestId()));
				}
			} else if (dialogActionId == SELECTED_QUEST_NOREWARD) {
				rewardIndex = javaSub(env.getExtendedRewardIndex(), 8);
				bool isLastRepeat = qs->getCompleteCount() == javaSub(template_->getRewardRepeatCount(), 1);
				if ((isLastRepeat && template_->isSingleTimeClassReward()) || template_->isClassRewardOnEveryRepeat()) {
					const std::vector<QuestItems>& byClass = template_->getSelectableRewardByClass(playerClass);
					if (rewardIndex >= 0 && rewardIndex < static_cast<int32_t>(byClass.size())) {
						questItems.push_back(byClass[static_cast<size_t>(rewardIndex)]);
					} else { // Java: warn(message, new Throwable()) - the Throwable only carries the stack trace into the log
						log.warn("The SelectableRewardByClass list has no element on index " + std::to_string(rewardIndex) + ". See quest id " +
									 std::to_string(env.getQuestId()),
							runtime::Exception(""));
					}
				}
			}
		}
		if (template_->getBonus() != nullptr) {
			// Handler can add additional bonuses on repeat (for event quests no data)
			questEngine::handlers::HandlerResult result =
				questEngine::QuestEngine::getInstance().onBonusApplyEvent(env, template_->getBonus()->getType(), questItems);
			if (result != questEngine::handlers::HandlerResult::FAILED) {
				std::optional<QuestItems> additional = reward::BonusService::getQuestBonus(*player, template_);
				if (additional)
					questItems.push_back(*additional);
			}
		}
	}

	return questItems;
}

int32_t QuestService::getRewardIndex(int32_t dialogActionId) {
	using model::DialogAction::SELECTED_QUEST_REWARD1;
	using model::DialogAction::SELECTED_QUEST_REWARD15;
	return dialogActionId >= SELECTED_QUEST_REWARD1 && dialogActionId <= SELECTED_QUEST_REWARD15 ? dialogActionId - SELECTED_QUEST_REWARD1 : -1;
}

void QuestService::giveReward(questEngine::model::QuestEnv& env, const model::templates::quest::Rewards* rewards) {
	using model::gameobjects::player::Rates;
	runtime::Ptr<model::gameobjects::player::Player> player = env.getPlayer();
	if (rewards->getKinah() != 0)
		player->getInventory().increaseKinah(calcResult(Rates::QUEST_KINAH, *player, rewards->getKinah()),
			item::ItemPacketService_ItemUpdateType::INC_KINAH_QUEST);
	if (rewards->getExp() != 0) {
		const model::templates::npc::NpcTemplate* npcTemplate = dataholders::DataManager::NPC_DATA->getNpcTemplate(env.getTargetId());
		std::optional<std::string> name = npcTemplate != nullptr ? std::optional<std::string>(npcTemplate->getL10n()) : std::nullopt;
		player->getCommonData()->addExp(rewards->getExp(), Rates::XP_QUEST, name ? std::optional<std::string_view>(*name) : std::nullopt);
	}
	if (rewards->getTitle() != 0)
		player->getTitleList().addTitle(rewards->getTitle(), true, 0);
	if (rewards->getAp() != 0) {
		int32_t ap = rewards->getAp();
		if (questTemplateOf(env.getQuestId())->getCategory() != model::templates::quest::QuestCategory::NON_COUNT) // don't multiply with quest rates for relic exchanges
			ap = calcResult(Rates::AP_QUEST, *player, ap);
		abyss::AbyssPointsService::addAp(*player, ap);
	}
	if (rewards->getDp() != 0)
		player->getCommonData()->addDp(rewards->getDp());
	if (rewards->getGp() != 0)
		abyss::GloryPointsService::addGp(player->getObjectId(), calcResult(Rates::GP, *player, rewards->getGp()));
	if (rewards->getExtendInventory() == 1)
		CubeExpandService::questExpand(*player);
	else if (rewards->getExtendInventory() == 2)
		WarehouseService::expand(*player, false);
}

std::optional<commons::database::Timestamp> QuestService::calculateRepeatDate(model::gameobjects::player::Player& player,
	const model::templates::QuestTemplate* template_) {
	using namespace std::chrono;
	using utils::time::ServerTime;
	using model::templates::quest::QuestRepeatCycle;
	// Java ServerTime.now(): the server zone's time at the installed clock (docs/deviations/P5-06a.md)
	ServerTime::ZonedDateTime now = ServerTime::ofEpochMilli(currentTimeMillis());
	// Java now.with(LocalTime.of(9, 0)): the same local date at 09:00:00.000, resolved in the server zone
	ServerTime::ZonedDateTime repeatDate = ServerTime::of(floor<days>(now.get_local_time()) + hours(9));
	if (now.get_sys_time() > repeatDate.get_sys_time()) // Java isAfter: the later instant
		repeatDate = ServerTime::of(repeatDate.get_local_time() + days(1)); // Java plusDays(1): on the local date-time, then resolved
	if (template_->isDaily()) {
		utils::PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_QUEST_LIMIT_START_DAILY(9));
	} else {
		weekday baseDay{floor<days>(repeatDate.get_local_time())}; // Java getDayOfWeek(): of the local date
		const std::optional<std::vector<QuestRepeatCycle>>& repeatCycle = template_->getRepeatCycle();
		if (!repeatCycle) // Java: findNextRepeatDay streams a null getRepeatCycle()
			throw runtime::NullPointerException("template.getRepeatCycle()");
		QuestRepeatCycle nextRepeatDay = findNextRepeatDay(*repeatCycle, baseDay);
		const int32_t baseDayValue = static_cast<int32_t>(baseDay.iso_encoding()); // Java DayOfWeek.getValue(): 1 (Monday) .. 7 (Sunday)
		if (getDay(nextRepeatDay) >= baseDayValue)
			repeatDate = ServerTime::of(repeatDate.get_local_time() + days(getDay(nextRepeatDay) - baseDayValue));
		else
			repeatDate = ServerTime::of(repeatDate.get_local_time() + days((7 - baseDayValue) + getDay(nextRepeatDay)));
		utils::PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_QUEST_LIMIT_START_WEEK(getL10n(nextRepeatDay), 9));
	}
	// Java new Timestamp(repeatDate.toEpochSecond() * 1000): the instant in whole seconds
	return commons::database::Timestamp(duration_cast<milliseconds>(floor<seconds>(repeatDate.get_sys_time()).time_since_epoch()));
}

model::templates::quest::QuestRepeatCycle QuestService::findNextRepeatDay(
	const std::vector<model::templates::quest::QuestRepeatCycle>& questRepeatDays, std::chrono::weekday day) {
	using model::templates::quest::QuestRepeatCycle;
	// Java: stream().sorted(Comparator.comparingInt(getDay)) - a stable sort
	std::vector<QuestRepeatCycle> resetDaysSorted = questRepeatDays;
	std::stable_sort(resetDaysSorted.begin(), resetDaysSorted.end(),
		[](QuestRepeatCycle a, QuestRepeatCycle b) { return getDay(a) < getDay(b); });
	for (QuestRepeatCycle resetDay : resetDaysSorted) {
		if (getDay(resetDay) >= static_cast<int32_t>(day.iso_encoding()))
			return resetDay;
	}
	return listGet(resetDaysSorted, 0);
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
	return startQuest(env, QuestStatus::START, env.getDialogActionId() != model::DialogAction::NULL_);
}

bool QuestService::startQuest(questEngine::model::QuestEnv& env, questEngine::model::QuestStatus status, bool warn) {
	runtime::Ptr<model::gameobjects::player::Player> player = env.getPlayer();
	int32_t id = env.getQuestId();
	runtime::Ptr<model::gameobjects::player::QuestStateList> qsl = player->getQuestStateList();
	runtime::Ptr<questEngine::model::QuestState> qs = qsl->getQuestState(id);
	const model::templates::QuestTemplate* template_ = questTemplateOf(id);
	if (template_->getNpcFactionId() != 0) {
		runtime::Ptr<model::gameobjects::player::npcFaction::NpcFaction> faction =
			player->getNpcFactions().getFactionById(template_->getNpcFactionId());
		if (!faction->isActive() || faction->getQuestId() != id) {
			utils::audit::AuditLogger::log(*player, "possibly used packet hack to start npc faction quest");
			return false;
		}
	}
	if (!checkStartConditions(*player, id, warn))
		return false;

	if (!template_->isNoCount() && !checkQuestListSize(*qsl) && !player->hasPermission(configs::main::MembershipConfig::QUEST_LIMIT_DISABLED.load())) {
		utils::PacketSendUtility::sendPacket(*player, SM_SYSTEM_MESSAGE::STR_QUEST_ACQUIRE_ERROR_MAX_NORMAL());
		return false;
	}

	SM_QUEST_ACTION::ActionType actionType;
	runtime::Ref<questEngine::model::QuestState> state; // Java's local qs from here on: the list's state or the new one
	if (qs) {
		actionType = qs->getStatus() == QuestStatus::COMPLETE ? SM_QUEST_ACTION::ActionType::ADD : SM_QUEST_ACTION::ActionType::UPDATE;
		qs->setStatus(status);
		state = runtime::Ref<questEngine::model::QuestState>(*qs);
	} else {
		actionType = SM_QUEST_ACTION::ActionType::ADD;
		state = questEngine::model::QuestState::create(id, status);
		player->getQuestStateList()->addQuest(id, *state);
	}

	if (template_->getNpcFactionId() != 0 && !template_->isTimeBased()) {
		player->getNpcFactions().startQuest(template_);
	}
	if (template_->getCategory() == model::templates::quest::QuestCategory::CHALLENGE_TASK)
		ChallengeTaskService::getInstance().onAcceptTask(*player, id);

	utils::PacketSendUtility::sendPacket(*player, SM_QUEST_ACTION(actionType, *state));
	player->getController().updateNearbyQuests();
	return true;
}

void QuestService::addOrUpdateQuest(model::gameobjects::player::Player& player, int32_t questId, questEngine::model::QuestStatus status) {
	SM_QUEST_ACTION::ActionType actionType;
	runtime::Ptr<questEngine::model::QuestState> qs = player.getQuestStateList()->getQuestState(questId);
	runtime::Ref<questEngine::model::QuestState> state; // Java's local qs from here on: the list's state or the new one
	if (!qs) {
		actionType = SM_QUEST_ACTION::ActionType::ADD;
		state = questEngine::model::QuestState::create(questId, status);
		player.getQuestStateList()->addQuest(questId, *state);
	} else {
		if (qs->getStatus() == status)
			return;
		actionType = qs->getStatus() == QuestStatus::COMPLETE ? SM_QUEST_ACTION::ActionType::ADD : SM_QUEST_ACTION::ActionType::UPDATE;
		qs->setStatus(status);
		if (status == QuestStatus::COMPLETE)
			qs->setQuestVar(0);
		state = runtime::Ref<questEngine::model::QuestState>(*qs);
	}
	utils::PacketSendUtility::sendPacket(player, SM_QUEST_ACTION(actionType, *state));
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
	int32_t id = env.getQuestId();
	runtime::Ptr<model::gameobjects::player::Player> player = env.getPlayer();
	const model::templates::QuestTemplate* template_ = questTemplateOf(id);
	if (template_->getCategory() != model::templates::quest::QuestCategory::EVENT)
		return false;

	if (!checkLevelRequirement(template_, player->getLevel()))
		return false;

	if (template_->getRacePermitted() && *template_->getRacePermitted() == player->getOppositeRace()) // Java: a null race is never equal
		return false;

	const std::vector<model::PlayerClass>& classPermitted = template_->getClassPermitted();
	if (!classPermitted.empty())
		if (std::ranges::find(classPermitted, player->getCommonData()->getPlayerClass()) == classPermitted.end())
			return false;

	if (template_->getGenderPermitted() && *template_->getGenderPermitted() != player->getGender())
		return false;

	runtime::Ptr<questEngine::model::QuestState> qs = player->getQuestStateList()->getQuestState(id);
	if (!qs) {
		runtime::Ref<questEngine::model::QuestState> created = questEngine::model::QuestState::create(template_->getId(), questStatus);
		player->getQuestStateList()->addQuest(id, *created);
	} else {
		qs->setStatus(questStatus);
		qs->setQuestVar(0);
		qs->setRewardGroup(std::nullopt);
	}
	return true;
}

bool QuestService::checkQuestListSize(model::gameobjects::player::QuestStateList& qsl) {
	// The player's quest list size + the new one to start
	return javaAdd(static_cast<int32_t>(qsl.getNormalQuests().size()), 1) <= configs::main::CustomConfig::BASIC_QUEST_SIZE_LIMIT.load();
}

bool QuestService::collectItemCheck(questEngine::model::QuestEnv& env, bool removeItem) {
	runtime::Ptr<model::gameobjects::player::Player> player = env.getPlayer();
	runtime::Ptr<questEngine::model::QuestState> qs = player->getQuestStateList()->getQuestState(env.getQuestId());
	if (!qs && removeItem)
		return false;
	const model::templates::QuestTemplate* template_ = questTemplateOf(env.getQuestId());
	const model::templates::quest::CollectItems* collectItems = template_->getCollectItems();
	if (collectItems == nullptr) {
		// check inventoryItems to prevent exploits
		const model::templates::quest::InventoryItems* inventoryItems = template_->getInventoryItems();
		if (inventoryItems == nullptr)
			return true;

		for (const model::templates::quest::InventoryItem& inventoryItem : inventoryItems->getInventoryItems()) {
			int32_t itemId = unboxed(inventoryItem.getItemId(), "inventoryItem.getItemId()");
			if (player->getInventory().getItemCountByItemId(itemId) < unboxed(inventoryItem.getCount(), "inventoryItem.getCount()"))
				return false;
		}

		if (removeItem) {
			for (const model::templates::quest::InventoryItem& inventoryItem : inventoryItems->getInventoryItems()) {
				player->getInventory().decreaseByItemId(unboxed(inventoryItem.getItemId(), "inventoryItem.getItemId()"),
					unboxed(inventoryItem.getCount(), "inventoryItem.getCount()"));
			}
		}
		return true;
	}

	for (const model::templates::quest::CollectItem& collectItem : collectItems->getCollectItem()) {
		int32_t itemId = unboxed(collectItem.getItemId(), "collectItem.getItemId()");
		int64_t count = itemId == model::items::ItemId::KINAH ? player->getInventory().getKinah() : player->getInventory().getItemCountByItemId(itemId);
		if (unboxed(collectItem.getCount(), "collectItem.getCount()") > count)
			return false;
	}
	if (removeItem) {
		for (const model::templates::quest::CollectItem& collectItem : collectItems->getCollectItem()) {
			if (unboxed(collectItem.getItemId(), "collectItem.getItemId()") == model::items::ItemId::KINAH)
				player->getInventory().decreaseKinah(unboxed(collectItem.getCount(), "collectItem.getCount()"));
			else {
				player->getInventory().decreaseByItemId(unboxed(collectItem.getItemId(), "collectItem.getItemId()"),
					unboxed(collectItem.getCount(), "collectItem.getCount()"));
			}
		}
	}
	return true;
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
	return checkAndGetCollectItemQuestRewardCategory(env, std::nullopt);
}

int32_t QuestService::checkAndGetCollectItemQuestRewardCategory(questEngine::model::QuestEnv& env, std::optional<int32_t> rewardIndex) {
	runtime::Ptr<model::gameobjects::player::Player> player = env.getPlayer();
	const model::templates::QuestTemplate* template_ = questTemplateOf(env.getQuestId());

	const model::templates::quest::CollectItems* collectItems = template_->getCollectItems();
	if (collectItems == nullptr || template_->getRewards().empty() ||
		(rewardIndex && *rewardIndex >= static_cast<int32_t>(template_->getRewards().size())))
		return -1;

	if (!rewardIndex) { // Verify if player has atleast one item with sufficient count and starts quest
		for (const model::templates::quest::CollectItem& cItem : collectItems->getCollectItem()) {
			if (player->getInventory().getItemCountByItemId(unboxed(cItem.getItemId(), "cItem.getItemId()")) >=
				unboxed(cItem.getCount(), "cItem.getCount()")) {
				runtime::Ptr<questEngine::model::QuestState> qs = player->getQuestStateList()->getQuestState(env.getQuestId());
				if (!qs || qs->isStartable()) {
					bool stateValid = true;
					if (collectItems->getStartCheck())
						stateValid = startQuest(env);
					if (stateValid)
						return 0;
				} else if (qs->getStatus() != QuestStatus::START && collectItems->getStartCheck()) {
					return -1;
				}
			}
		}
	} else {
		const model::templates::quest::CollectItem& selectedOption = listGet(collectItems->getCollectItem(), *rewardIndex);
		int32_t itemId = unboxed(selectedOption.getItemId(), "selectedOption.getItemId()");
		int32_t count = unboxed(selectedOption.getCount(), "selectedOption.getCount()");
		if (player->getInventory().getItemCountByItemId(itemId) < count || !player->getInventory().decreaseByItemId(itemId, count)) {
			const model::templates::item::ItemTemplate* itemTemplate = dataholders::DataManager::ITEM_DATA->getItemTemplate(itemId);
			if (itemTemplate == nullptr) // Java: getItemTemplate(itemId).getL10n() on null
				throw runtime::NullPointerException("ITEM_DATA.getItemTemplate(" + std::to_string(itemId) + ")");
			std::string requiredItemL10n = itemTemplate->getL10n();
			utils::PacketSendUtility::sendPacket(*player, SM_SYSTEM_MESSAGE::STR_MSG_QUEST_COMPLETE_ERROR_QUEST_ITEM_RETRY(requiredItemL10n));
			return -1;
		} else {
			return *rewardIndex;
		}
	}
	return -1;
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
	model::gameobjects::player::Player& player = *env.getPlayer();

	// Schedule Action When Timer Finishes (QuestService$1: the task pins the player it captured, fieldmap K3)
	runtime::FutureRef task = utils::ThreadPoolManager::getInstance().schedule({&player}, [&player] {
		questEngine::QuestEngine::getInstance().onQuestTimerEnd(*questEngine::model::QuestEnv::create(nullptr, player, 0));
	}, javaMul(timeInSeconds, 1000)); // Java: int timeInSeconds * 1000, widened to the long delay after the int product
	player.getController().addTask(model::TaskId::QUEST_TIMER, task);
	utils::PacketSendUtility::sendPacket(player, SM_QUEST_ACTION(env.getQuestId(), timeInSeconds));
	return true;
}

bool QuestService::invisibleTimerStart(questEngine::model::QuestEnv& env, int32_t timeInSeconds) {
	model::gameobjects::player::Player& player = *env.getPlayer();

	// Schedule Action When Timer Finishes (QuestService$2: the task pins the player it captured, fieldmap K3; Java keeps no handle)
	utils::ThreadPoolManager::getInstance().schedule({&player}, [&player] {
		questEngine::QuestEngine::getInstance().onInvisibleTimerEnd(*questEngine::model::QuestEnv::create(nullptr, player, 0));
	}, javaMul(timeInSeconds, 1000));
	return true;
}

bool QuestService::questTimerEnd(questEngine::model::QuestEnv& env) {
	model::gameobjects::player::Player& player = *env.getPlayer();

	player.getController().cancelTask(model::TaskId::QUEST_TIMER);
	utils::PacketSendUtility::sendPacket(player, SM_QUEST_ACTION(env.getQuestId(), 0));
	return true;
}

bool QuestService::abandonQuest(model::gameobjects::player::Player& player, int32_t questId) {
	const model::templates::QuestTemplate* template_ = dataholders::DataManager::QUEST_DATA->getQuestById(questId);
	if (template_ == nullptr)
		return false;

	if (template_->isCannotGiveup())
		return false;

	runtime::Ptr<questEngine::model::QuestState> found = player.getQuestStateList()->getQuestState(questId);
	if (!found || found->getStatus() == QuestStatus::COMPLETE || found->getStatus() == QuestStatus::LOCKED)
		return false;
	// Java's local qs outlives deleteQuest below, which removes the list's reference
	runtime::Ref<questEngine::model::QuestState> qs(*found);

	if (qs->getCompleteCount() > 0) { // set back to complete if it was completed at least once
		qs->setStatus(QuestStatus::COMPLETE, false);
		qs->setQuestVar(0);
		qs->setFlags(0);
	} else { // entirely delete from players quest list
		player.getQuestStateList()->deleteQuest(questId);
	}

	if (template_->getNpcFactionId() != 0)
		player.getNpcFactions().abortQuest(template_);

	removeQuestWorkItems(player, *qs);
	if (template_->getCategory() == model::templates::quest::QuestCategory::TASK) {
		const questEngine::handlers::models::XMLQuest* xmlQuest = dataholders::DataManager::XML_QUESTS->getQuest(questId);
		if (const auto* workOrders = dynamic_cast<const questEngine::handlers::models::WorkOrdersData*>(xmlQuest)) // Java instanceof
			player.getRecipeList()->deleteRecipe(player, workOrders->getRecipeId());
	}

	if (player.getController().hasTask(model::TaskId::QUEST_TIMER))
		questTimerEnd(*questEngine::model::QuestEnv::create(nullptr, player, questId));

	utils::PacketSendUtility::sendPacket(player, SM_QUEST_ACTION(SM_QUEST_ACTION::ActionType::ABANDON, *qs));
	player.getController().updateNearbyQuests();
	return true;
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
	std::vector<runtime::Ptr<model::gameobjects::player::Player>> players;
	for (const model::templates::quest::QuestDrop* qd : getQuestDrop(npcId)) {
		if (qd->isDropEachMemberGroup()) {
			for (const runtime::Ptr<model::gameobjects::AionObject>& object : group.getMembers()) {
				runtime::Ptr<model::gameobjects::player::Player> player = runtime::cast<model::gameobjects::player::Player>(object);
				runtime::Ptr<questEngine::model::QuestState> qstel = player->getQuestStateList()->getQuestState(questId);
				if (qstel && qstel->getStatus() == QuestStatus::START) {
					players.push_back(player);
				}
			}
			break;
		}
	}
	return players;
}

std::vector<runtime::Ptr<model::gameobjects::player::Player>> QuestService::getEachDropMembersAlliance(
	model::team::alliance::PlayerAlliance& alliance, int32_t npcId, int32_t questId) {
	std::vector<runtime::Ptr<model::gameobjects::player::Player>> players;
	for (const model::templates::quest::QuestDrop* qd : getQuestDrop(npcId)) {
		// Java asks the group flag here too (QuestService.java:922), not isDropEachMemberAlliance: ported as written
		if (qd->isDropEachMemberGroup()) {
			for (const runtime::Ptr<model::gameobjects::AionObject>& object : alliance.getMembers()) {
				runtime::Ptr<model::gameobjects::player::Player> player = runtime::cast<model::gameobjects::player::Player>(object);
				runtime::Ptr<questEngine::model::QuestState> qstel = player->getQuestStateList()->getQuestState(questId);
				if (qstel && qstel->getStatus() == QuestStatus::START) {
					players.push_back(player);
				}
			}
			break;
		}
	}
	return players;
}

void QuestService::removeQuestWorkItems(model::gameobjects::player::Player& player, questEngine::model::QuestState& qs) {
	const model::templates::quest::QuestWorkItems* qwi = questTemplateOf(qs.getQuestId())->getQuestWorkItems();
	if (qwi != nullptr) {
		// Java's `qi != null`: the C++ list holds values, so there is no null element to skip
		for (const model::templates::quest::QuestItems& qi : qwi->getQuestWorkItem()) {
			// Only remove the amount the quest actually needs, not the player's whole stack of that item
			int64_t count = std::min(qi.getCount(), player.getInventory().getItemCountByItemId(qi.getItemId()));
			if (count > 0)
				player.getInventory().decreaseByItemId(qi.getItemId(), count, qs.getStatus());
		}
	}
}

} // namespace aion::gameserver::services
