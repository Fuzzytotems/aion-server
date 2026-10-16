#include "aion/gameserver/questEngine/handlers/AbstractQuestHandler.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <string>
#include <vector>

#include "aion/gameserver/ai/AbstractAI.h"
#include "aion/gameserver/ai/event/AIEventType.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/controllers/PlayerController.h"
#include "aion/gameserver/controllers/VisibleObjectController.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/ItemData.h"
#include "aion/gameserver/dataholders/QuestsData.h"
#include "aion/gameserver/dataholders/detail/JavaHashMapOrder.h"
#include "aion/gameserver/dataholders/loadingutils/EnumTraits.h"
#include "aion/gameserver/model/CreatureType.h"
#include "aion/gameserver/model/DialogAction.h"
#include "aion/gameserver/model/DialogPageInfo.h"
#include "aion/gameserver/model/EmotionIdInfo.h"
#include "aion/gameserver/model/EmotionType.h"
#include "aion/gameserver/model/PlayerClass.h"
#include "aion/gameserver/model/TaskId.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/VisibleObject.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/QuestStateList.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/model/templates/QuestTemplate.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/model/templates/quest/CollectItem.h"
#include "aion/gameserver/model/templates/quest/CollectItems.h"
#include "aion/gameserver/model/templates/quest/FinishedQuestCond.h"
#include "aion/gameserver/model/templates/quest/QuestDrop.h"
#include "aion/gameserver/model/templates/quest/QuestItems.h"
#include "aion/gameserver/model/templates/quest/QuestNpc.h"
#include "aion/gameserver/model/templates/quest/QuestWorkItems.h"
#include "aion/gameserver/model/templates/quest/XMLStartCondition.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_DIALOG_WINDOW.h"
#include "aion/gameserver/network/aion/serverpackets/SM_EMOTION.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ITEM_USAGE_ANIMATION.h"
#include "aion/gameserver/network/aion/serverpackets/SM_NPC_INFO.h"
#include "aion/gameserver/network/aion/serverpackets/SM_PLAY_MOVIE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_QUEST_ACTION.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/questEngine/QuestEngine.h"
#include "aion/gameserver/questEngine/model/QuestActionType.h"
#include "aion/gameserver/questEngine/model/QuestEnv.h"
#include "aion/gameserver/questEngine/model/QuestState.h"
#include "aion/gameserver/questEngine/model/QuestStatus.h"
#include "aion/gameserver/questEngine/model/QuestVars.h"
#include "aion/gameserver/questEngine/task/QuestTasks.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/collections/HashSet.h"
#include "aion/gameserver/services/QuestService.h"
#include "aion/gameserver/services/item/ItemService.h"
#include "aion/gameserver/spawnengine/SpawnEngine.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/utils/PositionUtil.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"
#include "aion/gameserver/world/geo/GeoService.h"

namespace aion::gameserver::questEngine::handlers {

namespace {

namespace DialogAction = gameserver::model::DialogAction;
using gameserver::model::DialogPage;
using gameserver::model::gameobjects::Npc;
using gameserver::model::gameobjects::VisibleObject;
using gameserver::model::gameobjects::player::Player;
using gameserver::model::templates::QuestTemplate;
using model::QuestState;
using model::QuestStatus;
using network::aion::serverpackets::SM_DIALOG_WINDOW;
using network::aion::serverpackets::SM_ITEM_USAGE_ANIMATION;
using network::aion::serverpackets::SM_QUEST_ACTION;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;
using utils::PacketSendUtility;

/** Java int arithmetic (two's complement wrap-around): the handlers' step and time arguments reach these unchecked */
constexpr int32_t javaSub(int32_t a, int32_t b) noexcept {
	return static_cast<int32_t>(static_cast<uint32_t>(a) - static_cast<uint32_t>(b));
}

constexpr int32_t javaMul(int32_t a, int32_t b) noexcept {
	return static_cast<int32_t>(static_cast<uint32_t>(a) * static_cast<uint32_t>(b));
}

/** Java: DataManager.QUEST_DATA.getQuestById(questId) dereferenced right away (NullPointerException for an unknown quest) */
const QuestTemplate& questTemplateOf(int32_t questId) {
	const QuestTemplate* template_ = dataholders::DataManager::QUEST_DATA->getQuestById(questId);
	if (template_ == nullptr)
		throw runtime::NullPointerException("QUEST_DATA.getQuestById(" + std::to_string(questId) + ")");
	return *template_;
}

/** Java: an Integer unboxed to int (NullPointerException for null) */
int32_t unboxed(const std::optional<int32_t>& value, const char* what) {
	if (!value)
		throw runtime::NullPointerException(what);
	return *value;
}

/** Java: `env.getVisibleObject() instanceof Npc` (false for null) */
bool isNpc(model::QuestEnv& env) {
	return static_cast<bool>(runtime::as<Npc>(env.getVisibleObject()));
}

/** Java: DialogPage.SELECT_QUEST_REWARD_WINDOW1..10, the ten pages sendQuestDialog guards (AbstractQuestHandler.java:331-335) */
bool isRewardPage(int32_t dialogPageId) {
	using gameserver::model::id;
	return dialogPageId == id(DialogPage::SELECT_QUEST_REWARD_WINDOW1) || dialogPageId == id(DialogPage::SELECT_QUEST_REWARD_WINDOW2) ||
		dialogPageId == id(DialogPage::SELECT_QUEST_REWARD_WINDOW3) || dialogPageId == id(DialogPage::SELECT_QUEST_REWARD_WINDOW4) ||
		dialogPageId == id(DialogPage::SELECT_QUEST_REWARD_WINDOW5) || dialogPageId == id(DialogPage::SELECT_QUEST_REWARD_WINDOW6) ||
		dialogPageId == id(DialogPage::SELECT_QUEST_REWARD_WINDOW7) || dialogPageId == id(DialogPage::SELECT_QUEST_REWARD_WINDOW8) ||
		dialogPageId == id(DialogPage::SELECT_QUEST_REWARD_WINDOW9) || dialogPageId == id(DialogPage::SELECT_QUEST_REWARD_WINDOW10);
}

/** Java: Stream.of(PlayerClass.values()).allMatch(c -> quest.getSelectableRewardByClass(c).isEmpty()), in ordinal order */
bool hasNoSelectableRewardForAnyClass(const QuestTemplate& quest) {
	for (size_t ordinal = 0; ordinal < xml::EnumTraits<gameserver::model::PlayerClass>::names.size(); ++ordinal) {
		if (!quest.getSelectableRewardByClass(static_cast<gameserver::model::PlayerClass>(ordinal)).empty())
			return false;
	}
	return true;
}

/**
 * Java: `for (Integer questId : questNpc.getOnQuestStart())` (AbstractQuestHandler.java:438) iterates `new HashSet<>(0)` (QuestNpc.java:27), so
 * in the order of the ids in HashMap's table; the runtime HashSet keeps insertion order instead (MapOrder::HASH is unspecified, HashMap.h:25-27).
 * QuestNpc only ever adds to the set (QuestNpc.addOnQuestStart; QuestEngine.clear drops the whole QuestNpc), so replaying the insertions gives
 * Java's table, and that order decides the follow-up quest when several can start (docs/deviations/P5-06b.md). HashMap(0) starts at one
 * bucket (tableSizeFor(0) = 1) and doubles whenever the size exceeds the threshold - (int) (capacity * 0.75) below 16 buckets, twice the old one
 * from 16 up (HashMap.resize) - and a doubling keeps each bucket's relative order; add appends a new key to bucket (h ^ h >>> 16) & (n - 1)
 * (HashMap.hash of Integer.hashCode), and a bucket that already held 8 keys doubles a table below 64 buckets (treeifyBin). A bucket Java would
 * turn into a tree (8 keys in 64 or more buckets) iterates in another order: not modelled, it keeps the list order and is reported once per
 * process through noteJavaTreeifiedBucket, as dataholders::detail::JavaHashMapOrder (which models `new HashMap<>()`, 16 buckets) reports it.
 */
std::vector<int32_t> inJavaHashSetOrder(const runtime::HashSet<int32_t>& set) {
	constexpr size_t TREEIFY_THRESHOLD = 8;
	constexpr size_t MIN_TREEIFY_CAPACITY = 64;
	std::vector<std::vector<int32_t>> table;
	size_t threshold = 1; // HashMap(0): tableSizeFor(0)
	size_t size = 0;
	auto bucketOf = [](int32_t key, size_t capacity) {
		const uint32_t h = static_cast<uint32_t>(key); // Integer.hashCode()
		return static_cast<size_t>((h ^ (h >> 16)) & (capacity - 1));
	};
	auto resize = [&table, &threshold, &bucketOf] {
		const size_t oldCapacity = table.size();
		const size_t newCapacity = oldCapacity > 0 ? oldCapacity << 1 : threshold;
		const size_t newThreshold = oldCapacity >= 16 ? threshold << 1 : newCapacity * 3 / 4;
		std::vector<std::vector<int32_t>> newTable(newCapacity);
		for (const std::vector<int32_t>& bucket : table) {
			for (int32_t key : bucket)
				newTable[bucketOf(key, newCapacity)].push_back(key);
		}
		table = std::move(newTable);
		threshold = newThreshold;
	};
	for (int32_t questId : set.snapshot()) {
		if (table.empty())
			resize();
		std::vector<int32_t>& bucket = table[bucketOf(questId, table.size())];
		if (std::find(bucket.begin(), bucket.end(), questId) != bucket.end())
			continue; // Java HashSet.add of a present key changes nothing
		const size_t previousLength = bucket.size();
		bucket.push_back(questId);
		if (previousLength >= TREEIFY_THRESHOLD) { // putVal: binCount >= TREEIFY_THRESHOLD - 1
			if (table.size() < MIN_TREEIFY_CAPACITY)
				resize();
			else
				dataholders::detail::noteJavaTreeifiedBucket();
		}
		if (++size > threshold)
			resize();
	}
	std::vector<int32_t> order;
	order.reserve(size);
	for (const std::vector<int32_t>& bucket : table)
		order.insert(order.end(), bucket.begin(), bucket.end());
	return order;
}

/**
 * Java: removeQuestItem(env, itemId, itemCount, null), which sendQuestEndDialog(env, int[]) calls for a player without the quest
 * (AbstractQuestHandler.java:404-406): PlayerStorage.decreaseByItemId(itemId, count, null) hands its actor, the player, on
 * (PlayerStorage.java:122-124), and a null status makes Storage take the delete type from DEC_ITEM_USE (Storage.java:141). The declared
 * removeQuestItem takes a QuestStatus value, so the null arm is spelled here with the same statements.
 */
bool removeQuestItemWithoutStatus(runtime::Ptr<Player> player, int32_t itemId, int64_t itemCount) {
	if (itemId != 0 && itemCount != 0)
		return player->getInventory().decreaseByItemId(itemId, itemCount, std::optional<QuestStatus>(), player);
	return false;
}

} // namespace

AbstractQuestHandler::AbstractQuestHandler(int32_t questIdValue) : qe(QuestEngine::getInstance()), questId(questIdValue) {
	const gameserver::model::templates::QuestTemplate* template_ = dataholders::DataManager::QUEST_DATA->getQuestById(questId);
	if (template_ != nullptr) { // Some artificial quests have dummy questIds
		loadWorkItems(template_);
		loadActionItems(template_);
	}
}

AbstractQuestHandler::~AbstractQuestHandler() = default;

void AbstractQuestHandler::loadWorkItems(const gameserver::model::templates::QuestTemplate* template_) {
	const gameserver::model::templates::quest::QuestWorkItems* questWorkItems = template_->getQuestWorkItems();
	if (questWorkItems != nullptr) {
		// Java keeps the template's list; C++: the runtime list of pointers to the immortal template items
		runtime::Ref<runtime::RcArrayList<const gameserver::model::templates::quest::QuestItems*>> items =
			runtime::RcArrayList<const gameserver::model::templates::quest::QuestItems*>::create(AION_LOCK_CLASS(AbstractQuestHandler::workItems));
		for (const gameserver::model::templates::quest::QuestItems& item : questWorkItems->getQuestWorkItem())
			items->add(&item);
		workItems.set(items);
	}
}

void AbstractQuestHandler::loadActionItems(const gameserver::model::templates::QuestTemplate* template_) {
	for (const gameserver::model::templates::quest::QuestDrop& drop : template_->getQuestDrop()) {
		if (!drop.getNpcId())
			throw runtime::NullPointerException("QuestDrop.npcId"); // Java: unboxing of a null Integer
		int32_t npcId = *drop.getNpcId();
		if (npcId / 100000 != 7)
			continue;
		if (!actionItems.get())
			actionItems.set(runtime::RcHashSet<int32_t>::create(AION_LOCK_CLASS(AbstractQuestHandler::actionItems)));
		actionItems.get()->add(npcId);
	}
}

std::unordered_set<int32_t> AbstractQuestHandler::getActionItems() {
	runtime::Ptr<runtime::RcHashSet<int32_t>> items = actionItems.get();
	if (!items)
		return {}; // Java: Collections.emptySet()
	// Java: Collections.unmodifiableSet(actionItems), a read-only view; C++: a copy (only the constructor fills the set)
	std::unordered_set<int32_t> view;
	for (int32_t npcId : items->snapshot())
		view.insert(npcId);
	return view;
}

bool AbstractQuestHandler::onDialogEvent(model::QuestEnv& env) {
	switch (env.getDialogActionId()) {
		case DialogAction::ASK_QUEST_ACCEPT: // show quest accept dialog (coming from pre-conversation)
			sendDialogPacket(env, gameserver::model::id(DialogPage::ASK_QUEST_ACCEPT_WINDOW), questId);
			return true;
		case DialogAction::QUEST_ACCEPT_1:
			return isNpc(env) ? sendQuestDialog(env, 1003) : closeDialogWindow(env);
		case DialogAction::QUEST_REFUSE:
		case DialogAction::QUEST_REFUSE_SIMPLE:
		case DialogAction::QUEST_REFUSE_1:
			return isNpc(env) ? sendQuestDialog(env, 1004) : closeDialogWindow(env);
		case DialogAction::QUEST_REFUSE_2:
			return isNpc(env) ? sendQuestDialog(env, 1005) : closeDialogWindow(env);
		case DialogAction::QUEST_REFUSE_3:
			return isNpc(env) ? sendQuestDialog(env, 1006) : closeDialogWindow(env);
		case DialogAction::QUEST_REFUSE_4:
			return isNpc(env) ? sendQuestDialog(env, 1007) : closeDialogWindow(env);
		case DialogAction::FINISH_DIALOG: // clicking X or ^ in quest accept window (client closes the window by itself / returns to quest selection)
			return true;
	}
	return false;
}

bool AbstractQuestHandler::onCanAct(model::QuestEnv& env, model::QuestActionType questEventType, std::span<const std::any> objects) {
	static_cast<void>(objects); // Java: Object... objects, unused here
	runtime::Ptr<Player> player = env.getPlayer();
	runtime::Ptr<QuestState> qs = player->getQuestStateList()->getQuestState(env.getQuestId());
	if (!qs || qs->getStatus() != QuestStatus::START)
		return false;
	if (questEventType == model::QuestActionType::ACTION_ITEM_USE && actionItems.get()) {
		const QuestTemplate& template_ = questTemplateOf(env.getQuestId());
		int32_t droppedItem = 0;
		int32_t dropCount = 0;
		for (const gameserver::model::templates::quest::QuestDrop& drop : template_.getQuestDrop()) {
			if (unboxed(drop.getNpcId(), "QuestDrop.npcId") == env.getTargetId()) {
				droppedItem = unboxed(drop.getItemId(), "QuestDrop.itemId");
				break;
			}
		}
		const gameserver::model::templates::quest::CollectItems* collectItems = template_.getCollectItems();
		if (collectItems != nullptr && droppedItem != 0) {
			for (const gameserver::model::templates::quest::CollectItem& item : collectItems->getCollectItem()) {
				if (unboxed(item.getItemId(), "CollectItem.itemId") == droppedItem) {
					dropCount = unboxed(item.getCount(), "CollectItem.count");
					break;
				}
			}
			if (dropCount != 0) {
				int64_t currentCount = player->getInventory().getItemCountByItemId(droppedItem);
				if (currentCount >= dropCount)
					return false;
			}
		}
	}
	return true;
}

void AbstractQuestHandler::updateQuestStatus(model::QuestEnv& env) {
	runtime::Ptr<Player> player = env.getPlayer();
	runtime::Ptr<QuestState> qs = player->getQuestStateList()->getQuestState(questId);
	PacketSendUtility::sendPacket(*player, SM_QUEST_ACTION(SM_QUEST_ACTION::ActionType::UPDATE, *qs));
	if (qs->getStatus() == QuestStatus::COMPLETE || qs->getStatus() == QuestStatus::REWARD)
		player->getController().updateNearbyQuests();
}

bool AbstractQuestHandler::changeQuestStep(model::QuestEnv& env, int32_t oldStep, int32_t newStep) {
	return changeQuestStep(env, oldStep, newStep, false, oldStep > 0x3F || newStep > 0x3F ? -1 : 0);
}

bool AbstractQuestHandler::changeQuestStep(model::QuestEnv& env, int32_t step, int32_t nextStep, bool reward) {
	return changeQuestStep(env, step, nextStep, reward, 0);
}

bool AbstractQuestHandler::changeQuestStep(model::QuestEnv& env, int32_t step, int32_t nextStep, bool reward, int32_t varNum) {
	runtime::Ptr<QuestState> qs = env.getPlayer()->getQuestStateList()->getQuestState(questId);
	if (qs && (varNum == -1 ? qs->getQuestVars()->getQuestVars() == step : qs->getQuestVarById(varNum) == step)) {
		if (nextStep != step) { // quest can be rolled back if nextStep < step
			if (step > nextStep && qs->getStatus() == QuestStatus::START)
				PacketSendUtility::sendPacket(*env.getPlayer(), SM_SYSTEM_MESSAGE::STR_QUEST_SYSTEMMSG_GIVEUP(questTemplateOf(questId).getL10n()));
			if (varNum == -1)
				qs->setQuestVar(nextStep);
			else
				qs->setQuestVarById(varNum, nextStep);
		}
		if (reward || nextStep != step) {
			if (reward)
				qs->setStatus(QuestStatus::REWARD);
			updateQuestStatus(env);
		}
		return true;
	}
	return false;
}

bool AbstractQuestHandler::sendQuestDialog(model::QuestEnv& env, int32_t dialogPageId) {
	if (isRewardPage(dialogPageId)) {
		runtime::Ptr<QuestState> qs = env.getPlayer()->getQuestStateList()->getQuestState(questId);
		if (!qs || qs->getStatus() != QuestStatus::REWARD) // reward packet exploitation fix
			return false;
	}
	// Not using handler questId, because some quests may handle events when quests are finished
	// In that case questId must be zero!!! (Kromede entry for example)
	sendDialogPacket(env, dialogPageId, env.getQuestId());
	return true;
}

void AbstractQuestHandler::sendDialogPacket(model::QuestEnv& env, int32_t dialogPageId, int32_t value) {
	int32_t objId = 0;
	if (runtime::Ptr<VisibleObject> visibleObject = env.getVisibleObject()) {
		objId = visibleObject->getObjectId();
	}
	PacketSendUtility::sendPacket(*env.getPlayer(), SM_DIALOG_WINDOW(objId, dialogPageId, value));
}

bool AbstractQuestHandler::sendQuestSelectionDialog(model::QuestEnv& env) {
	sendDialogPacket(env, 10, 0);
	return true;
}

bool AbstractQuestHandler::closeDialogWindow(model::QuestEnv& env) {
	sendDialogPacket(env, 0, 0);
	return true;
}

bool AbstractQuestHandler::sendQuestStartDialog(model::QuestEnv& env) {
	return sendQuestStartDialog(env, 0, 0); // TODO remove all calls and replace with super.onDialogEvent()
}

bool AbstractQuestHandler::sendQuestStartDialog(model::QuestEnv& env, const gameserver::model::templates::quest::QuestItems* workItem) {
	return workItem == nullptr ? sendQuestStartDialog(env, 0, 0) : sendQuestStartDialog(env, workItem->getItemId(), workItem->getCount());
}

bool AbstractQuestHandler::sendQuestStartDialog(model::QuestEnv& env, int32_t itemId, int64_t itemCount) {
	switch (env.getDialogActionId()) {
		case DialogAction::ASK_QUEST_ACCEPT:
			return sendQuestDialog(env, 4);
		case DialogAction::QUEST_ACCEPT:
		case DialogAction::QUEST_ACCEPT_1:
		case DialogAction::QUEST_ACCEPT_SIMPLE:
			if (services::QuestService::startQuest(env)) {
				if (itemId != 0 && itemCount != 0)
					giveQuestItem(env, itemId, itemCount);
				if (env.getDialogActionId() != DialogAction::QUEST_ACCEPT_SIMPLE && isNpc(env))
					return sendQuestDialog(env, 1003);
				else
					return closeDialogWindow(env);
			}
			break;
		case DialogAction::QUEST_REFUSE_1:
		case DialogAction::QUEST_REFUSE_2:
			return sendQuestDialog(env, 1004);
		case DialogAction::QUEST_REFUSE_SIMPLE:
			return closeDialogWindow(env);
		case DialogAction::FINISH_DIALOG:
			return sendQuestSelectionDialog(env);
	}
	return false;
}

bool AbstractQuestHandler::sendQuestEndDialog(model::QuestEnv& env, std::span<const int32_t> questItemsToRemove) {
	runtime::Ptr<Player> player = env.getPlayer();
	runtime::Ptr<QuestState> qs = player->getQuestStateList()->getQuestState(questId);
	std::optional<QuestStatus> status = !qs ? std::nullopt : std::optional<QuestStatus>(qs->getStatus());
	for (int32_t itemId : questItemsToRemove) {
		int64_t itemCount = player->getInventory().getItemCountByItemId(itemId);
		if (status)
			removeQuestItem(env, itemId, itemCount, *status);
		else
			removeQuestItemWithoutStatus(env.getPlayer(), itemId, itemCount);
	}
	return sendQuestEndDialog(env);
}

bool AbstractQuestHandler::sendQuestEndDialog(model::QuestEnv& env) {
	runtime::Ptr<Player> player = env.getPlayer();
	runtime::Ptr<QuestState> qs = player->getQuestStateList()->getQuestState(questId);
	if (!qs || qs->getStatus() != QuestStatus::REWARD)
		return false; // reward packet exploitation fix (or buggy quest handler)

	int32_t dialogActionId = env.getDialogActionId();
	if (dialogActionId >= DialogAction::SELECTED_QUEST_REWARD1 && dialogActionId <= DialogAction::SELECTED_QUEST_NOREWARD) {
		if (services::QuestService::finishQuest(env)) {
			runtime::Ptr<Npc> npc = runtime::cast<Npc>(env.getVisibleObject()); // Java: (Npc) env.getVisibleObject()
			runtime::Ref<gameserver::model::templates::quest::QuestNpc> questNpc = qe.getQuestNpc(npc->getNpcId());
			bool npcHasActiveQuest = false;
			for (int32_t talkQuestId : questNpc->getOnTalkEvent()) { // all quest IDs that have registered talk events for this npc
				runtime::Ptr<QuestState> qs2 = player->getQuestStateList()->getQuestState(talkQuestId);
				if (qs2 && qs2->getStatus() == QuestStatus::REWARD) { // TODO make sure that this npc is the end npc
					env.setQuestId(talkQuestId);
					env.setDialogActionId(DialogAction::USE_OBJECT); // show default dialog (reward selection for next quest)
					return qe.onDialog(*model::QuestEnv::create(npc, *player, talkQuestId, DialogAction::USE_OBJECT));
				} else if (!npcHasActiveQuest && qs2 && qs2->getStatus() == QuestStatus::START) {
					bool isQuestStartNpc = questNpc->getOnQuestStart().contains(talkQuestId);
					if (!isQuestStartNpc || (questTemplateOf(talkQuestId).isMission() && qs2->getQuestVars()->getQuestVars() == 0))
						npcHasActiveQuest = true; // TODO correct way to make sure that active quest can be continued at this npc
				}
			}
			bool npcHasNewQuest = false;
			for (int32_t startQuestId : inJavaHashSetOrder(questNpc->getOnQuestStart())) { // all quest IDs that are registered to be started at this npc
				if (services::QuestService::checkStartConditions(*player, startQuestId, false)) {
					npcHasNewQuest = true;
					const QuestTemplate& template_ = questTemplateOf(startQuestId);
					for (const gameserver::model::templates::quest::XMLStartCondition& startCondition : template_.getXMLStartConditions()) {
						// Java: a null list without <finished>; C++: an empty one
						for (const gameserver::model::templates::quest::FinishedQuestCond& fcondition : startCondition.getFinishedPreconditions()) {
							if (fcondition.getQuestId() == env.getQuestId() && isAcceptableQuest(&template_)) {
								env.setQuestId(startQuestId);
								env.setDialogActionId(DialogAction::QUEST_SELECT);
								env.setDialogContinuationFromPreQuest(true);
								return qe.onDialog(env); // show start dialog of follow-up quest
							}
						}
					}
				}
			}
			return npcHasActiveQuest || npcHasNewQuest ? sendQuestSelectionDialog(env) : closeDialogWindow(env);
		}
	} else {
		switch (dialogActionId) {
			case DialogAction::SET_SUCCEED: // report to pre-end npc (another npc is actually responsible for rewarding, so close this window)
				return closeDialogWindow(env);
			case DialogAction::USE_OBJECT: // start talking to npc
			case DialogAction::QUEST_SELECT: // start talking to npc
			case DialogAction::SELECT_QUEST_REWARD: // report to end npc
			case DialogAction::CHECK_USER_HAS_QUEST_ITEM: // report to end npc with collect item checks
			case DialogAction::CHECK_USER_HAS_QUEST_ITEM_SIMPLE: // report to end npc with collect item checks
				services::QuestService::validateAndFixRewardGroup(qs, questId); // fixes the reward group if necessary
				// show reward selection page
				return sendQuestDialog(env, gameserver::model::id(gameserver::model::getRewardPageByIndex(qs->getRewardGroup())));
		}
	}
	return false;
}

bool AbstractQuestHandler::isAcceptableQuest(const gameserver::model::templates::QuestTemplate* quest) {
	if (quest == nullptr)
		throw runtime::NullPointerException("quest");
	if (quest->getMinlevelPermitted() == 99)
		return false;
	if (quest->getRewards().empty() && quest->getExtendedRewards() == nullptr && quest->getBonus() == nullptr && quest->getQuestDrop().empty() &&
		hasNoSelectableRewardForAnyClass(*quest)) {
		return false;
	}
	return true;
}

bool AbstractQuestHandler::defaultCloseDialog(model::QuestEnv& env, int32_t step, int32_t nextStep) {
	return defaultCloseDialog(env, step, nextStep, false, false, 0, 0, 0, 0);
}

bool AbstractQuestHandler::defaultCloseDialog(model::QuestEnv& env, int32_t step, int32_t nextStep, int32_t giveItemId, int64_t giveItemCount) {
	return defaultCloseDialog(env, step, nextStep, false, false, giveItemId, giveItemCount, 0, 0);
}

bool AbstractQuestHandler::defaultCloseDialog(model::QuestEnv& env, int32_t step, int32_t nextStep, bool reward, bool sameNpc) {
	return defaultCloseDialog(env, step, nextStep, reward, sameNpc, 0, 0, 0, 0);
}

bool AbstractQuestHandler::defaultCloseDialog(model::QuestEnv& env, int32_t step, int32_t nextStep, bool reward, bool sameNpc,
	const gameserver::model::templates::quest::QuestItems* questItemToAdd) {
	if (questItemToAdd == nullptr)
		throw runtime::NullPointerException("questItemToAdd");
	return defaultCloseDialog(env, step, nextStep, reward, sameNpc, questItemToAdd->getItemId(), questItemToAdd->getCount(), 0, 0);
}

bool AbstractQuestHandler::defaultCloseDialog(model::QuestEnv& env, int32_t step, int32_t nextStep, int32_t giveItemId, int64_t giveItemCount,
	int32_t removeItemId, int64_t removeItemCount) {
	return defaultCloseDialog(env, step, nextStep, false, false, giveItemId, giveItemCount, removeItemId, removeItemCount);
}

bool AbstractQuestHandler::defaultCloseDialog(model::QuestEnv& env, int32_t step, int32_t nextStep, bool reward, bool sameNpc, int32_t giveItemId,
	int64_t giveItemCount, int32_t removeItemId, int64_t removeItemCount) {
	runtime::Ptr<QuestState> qs = env.getPlayer()->getQuestStateList()->getQuestState(questId);
	if (qs->getQuestVarById(0) == step) {
		if (giveItemId != 0 && giveItemCount != 0) {
			if (!giveQuestItem(env, giveItemId, giveItemCount)) {
				return false;
			}
		}
		removeQuestItem(env, removeItemId, removeItemCount, qs->getStatus());
		changeQuestStep(env, step, nextStep, reward);
		if (sameNpc) {
			return sendQuestEndDialog(env);
		}
		if (runtime::Ptr<Npc> npc = runtime::as<Npc>(env.getVisibleObject()))
			npc->getAi().onCreatureEvent(ai::event::AIEventType::DIALOG_FINISH, *env.getPlayer());
		return closeDialogWindow(env);
	}
	return false;
}

bool AbstractQuestHandler::checkQuestItems(model::QuestEnv& env, int32_t step, int32_t nextStep, bool reward, int32_t checkOkId,
	int32_t checkFailId) {
	return checkQuestItems(env, step, nextStep, reward, checkOkId, checkFailId, 0, 0);
}

bool AbstractQuestHandler::checkQuestItems(model::QuestEnv& env, int32_t step, int32_t nextStep, bool reward, int32_t checkOkId, int32_t checkFailId,
	int32_t giveItemId, int32_t giveItemCount) {
	runtime::Ptr<Player> player = env.getPlayer();
	runtime::Ptr<QuestState> qs = player->getQuestStateList()->getQuestState(questId);
	if (qs->getQuestVarById(0) == step) {
		if (services::QuestService::collectItemCheck(env, true)) {
			if (giveItemId != 0 && giveItemCount != 0) {
				if (!giveQuestItem(env, giveItemId, giveItemCount)) {
					return false;
				}
			}
			changeQuestStep(env, step, nextStep, reward);
			return sendQuestDialog(env, checkOkId);
		} else {
			return sendQuestDialog(env, checkFailId);
		}
	}
	return false;
}

bool AbstractQuestHandler::checkQuestItemsSimple(model::QuestEnv& env, int32_t step, int32_t nextStep, bool reward, int32_t checkOkId,
	int32_t giveItemId, int32_t giveItemCount) {
	runtime::Ptr<Player> player = env.getPlayer();
	runtime::Ptr<QuestState> qs = player->getQuestStateList()->getQuestState(questId);
	if (qs->getQuestVarById(0) == step) {
		if (services::QuestService::collectItemCheck(env, true)) {
			if (giveItemId != 0 && giveItemCount != 0) {
				if (!giveQuestItem(env, giveItemId, giveItemCount)) {
					return false;
				}
			}
			changeQuestStep(env, step, nextStep, reward);
			return sendQuestDialog(env, checkOkId);
		} else
			return closeDialogWindow(env);
	}
	return false;
}

bool AbstractQuestHandler::checkItemExistence(model::QuestEnv& env, int32_t step, int32_t nextStep, bool reward, int32_t itemId, int32_t itemCount,
	bool remove, int32_t checkOkId, int32_t checkFailId, int32_t giveItemId, int32_t giveItemCount) {
	runtime::Ptr<Player> player = env.getPlayer();
	runtime::Ptr<QuestState> qs = player->getQuestStateList()->getQuestState(questId);
	if (qs->getQuestVarById(0) == step) {
		if (checkItemExistence(env, itemId, itemCount, remove)) {
			if (giveItemId != 0 && giveItemCount != 0) {
				if (!giveQuestItem(env, giveItemId, giveItemCount)) {
					return false;
				}
			}
			changeQuestStep(env, step, nextStep, reward);
			return sendQuestDialog(env, checkOkId);
		} else {
			return sendQuestDialog(env, checkFailId);
		}
	}
	return false;
}

bool AbstractQuestHandler::checkItemExistence(model::QuestEnv& env, int32_t itemId, int32_t itemCount, bool remove) {
	runtime::Ptr<Player> player = env.getPlayer();
	if (player->getInventory().getItemCountByItemId(itemId) >= itemCount) {
		if (remove) {
			if (!removeQuestItem(env, itemId, itemCount)) {
				return false;
			}
		}
		return true;
	} else {
		return false;
	}
}

void AbstractQuestHandler::sendEmotion(model::QuestEnv& env, gameserver::model::gameobjects::Creature& emoteCreature,
	gameserver::model::EmotionId emotion, bool broadcast) {
	runtime::Ptr<Player> player = env.getPlayer();
	int32_t targetId = player->equals(emoteCreature) ? env.getVisibleObject()->getObjectId() : player->getObjectId();
	PacketSendUtility::broadcastPacket(*player,
		network::aion::serverpackets::SM_EMOTION(emoteCreature, gameserver::model::EmotionType::EMOTE, gameserver::model::id(emotion), targetId),
		broadcast);
}

bool AbstractQuestHandler::giveQuestItem(model::QuestEnv& env, int32_t itemId, int64_t itemCount) {
	return giveQuestItem(env, itemId, itemCount, services::item::ItemPacketService_ItemAddType::QUEST_WORK_ITEM,
		services::item::ItemPacketService_ItemUpdateType::INC_ITEM_COLLECT);
}

bool AbstractQuestHandler::giveQuestItem(model::QuestEnv& env, int32_t itemId, int64_t itemCount,
	services::item::ItemPacketService_ItemAddType addType) {
	return giveQuestItem(env, itemId, itemCount, addType, services::item::ItemPacketService_ItemUpdateType::INC_ITEM_COLLECT);
}

bool AbstractQuestHandler::giveQuestItem(model::QuestEnv& env, int32_t itemId, int64_t itemCount,
	services::item::ItemPacketService_ItemAddType addType, services::item::ItemPacketService_ItemUpdateType updateType) {
	runtime::Ptr<Player> player = env.getPlayer();
	const gameserver::model::templates::item::ItemTemplate* item = dataholders::DataManager::ITEM_DATA->getItemTemplate(itemId);
	if (itemId != 0 && itemCount != 0) {
		int64_t existentItemCount = player->getInventory().getItemCountByItemId(itemId);
		if (existentItemCount < itemCount) {
			int64_t itemsToGive = itemCount - existentItemCount; // some quest work items come from multiple quests, don't add again
			services::item::ItemService::addItem(*player, itemId, itemsToGive, true,
				*services::item::ItemService::ItemUpdatePredicate::create(addType, updateType));
			return true;
		} else {
			if (item == nullptr) // Java: item.getL10n() on the null template of an unknown item id
				throw runtime::NullPointerException("ITEM_DATA.getItemTemplate(" + std::to_string(itemId) + ")");
			PacketSendUtility::sendPacket(*player, SM_SYSTEM_MESSAGE::STR_CAN_NOT_GET_LORE_ITEM((item->getL10n())));
			return true;
		}
	}
	return false;
}

bool AbstractQuestHandler::removeQuestItem(model::QuestEnv& env, int32_t itemId, int64_t itemCount) {
	runtime::Ptr<Player> player = env.getPlayer();
	if (itemId != 0 && itemCount > 0) {
		runtime::Ptr<QuestState> qs = player->getQuestStateList()->getQuestState(questId);
		return player->getInventory().decreaseByItemId(itemId, itemCount, !qs ? QuestStatus::START : qs->getStatus());
	}
	return false;
}

bool AbstractQuestHandler::removeQuestItem(model::QuestEnv& env, int32_t itemId, int64_t itemCount, model::QuestStatus questStatus) {
	runtime::Ptr<Player> player = env.getPlayer();
	if (itemId != 0 && itemCount != 0) {
		return player->getInventory().decreaseByItemId(itemId, itemCount, questStatus);
	}
	return false;
}

bool AbstractQuestHandler::playQuestMovie(model::QuestEnv& env, int32_t movieId) {
	return playQuestMovie(env, movieId, false);
}

bool AbstractQuestHandler::playQuestMovie(model::QuestEnv& env, int32_t cutsceneId, bool isCutsceneMovie) {
	int32_t targetObjectId = !env.getVisibleObject() ? 0 : env.getVisibleObject()->getObjectId();
	PacketSendUtility::sendPacket(*env.getPlayer(),
		network::aion::serverpackets::SM_PLAY_MOVIE(isCutsceneMovie, targetObjectId, env.getQuestId(), cutsceneId, true));
	return false;
}

bool AbstractQuestHandler::defaultOnKillEvent(model::QuestEnv& env, int32_t npcId, int32_t startVar, int32_t endVar) {
	const std::array<int32_t, 1> mobids{npcId};
	return defaultOnKillEvent(env, mobids, startVar, endVar);
}

bool AbstractQuestHandler::defaultOnKillEvent(model::QuestEnv& env, std::span<const int32_t> npcIds, int32_t startVar, int32_t endVar) {
	return defaultOnKillEvent(env, npcIds, startVar, endVar, 0);
}

bool AbstractQuestHandler::defaultOnKillEvent(model::QuestEnv& env, int32_t npcId, int32_t startVar, int32_t endVar, int32_t varNum) {
	const std::array<int32_t, 1> mobids{npcId};
	return defaultOnKillEvent(env, mobids, startVar, endVar, varNum);
}

bool AbstractQuestHandler::defaultOnKillEvent(model::QuestEnv& env, std::span<const int32_t> npcIds, int32_t startVar, int32_t endVar,
	int32_t varNum) {
	runtime::Ptr<Player> player = env.getPlayer();
	runtime::Ptr<QuestState> qs = player->getQuestStateList()->getQuestState(questId);
	if (qs && qs->getStatus() == QuestStatus::START) {
		int32_t var = qs->getQuestVarById(varNum);
		int32_t targetId = env.getTargetId();
		for (int32_t id : npcIds) {
			if (targetId == id) {
				if (var >= startVar && var < endVar) {
					qs->setQuestVarById(varNum, var + 1);
					updateQuestStatus(env);
					return true;
				}
			}
		}
	}
	return false;
}

bool AbstractQuestHandler::defaultOnKillEvent(model::QuestEnv& env, int32_t npcId, int32_t startVar, bool reward) {
	const std::array<int32_t, 1> mobids{npcId};
	return (defaultOnKillEvent(env, mobids, startVar, reward, 0));
}

bool AbstractQuestHandler::defaultOnKillEvent(model::QuestEnv& env, int32_t npcId, int32_t startVar, bool reward, int32_t varNum) {
	const std::array<int32_t, 1> mobids{npcId};
	return (defaultOnKillEvent(env, mobids, startVar, reward, varNum));
}

bool AbstractQuestHandler::defaultOnKillEvent(model::QuestEnv& env, std::span<const int32_t> npcIds, int32_t startVar, bool reward) {
	return (defaultOnKillEvent(env, npcIds, startVar, reward, 0));
}

bool AbstractQuestHandler::defaultOnKillEvent(model::QuestEnv& env, std::span<const int32_t> npcIds, int32_t startVar, bool reward, int32_t varNum) {
	runtime::Ptr<Player> player = env.getPlayer();
	runtime::Ptr<QuestState> qs = player->getQuestStateList()->getQuestState(questId);
	if (qs && qs->getStatus() == QuestStatus::START) {
		int32_t var = qs->getQuestVarById(varNum);
		int32_t targetId = env.getTargetId();
		for (int32_t id : npcIds) {
			if (targetId == id) {
				if (var == startVar) {
					if (reward) {
						qs->setStatus(QuestStatus::REWARD);
					} else {
						qs->setQuestVarById(varNum, var + 1);
					}
					updateQuestStatus(env);
					return true;
				}
			}
		}
	}
	return false;
}

bool AbstractQuestHandler::defaultOnKillRankedEvent(model::QuestEnv& env, int32_t startVar, int32_t endVar, bool reward) {
	return defaultOnKillRankedEvent(env, startVar, endVar, reward, false);
}

bool AbstractQuestHandler::defaultOnKillRankedEvent(model::QuestEnv& env, int32_t startVar, int32_t endVar, bool reward, bool isDataDriven) {
	runtime::Ptr<Player> player = env.getPlayer();
	runtime::Ptr<QuestState> qs = player->getQuestStateList()->getQuestState(questId);
	if (qs && qs->getStatus() == QuestStatus::START) {
		int32_t var = qs->getQuestVarById(0);
		if (isDataDriven) {
			int32_t varKill = qs->getQuestVarById(1);
			if (varKill >= startVar && varKill < javaSub(endVar, 1)) {
				changeQuestStep(env, varKill, varKill + 1, false, 1);
				return true;
			} else if (varKill == javaSub(endVar, 1)) {
				if (reward)
					qs->setStatus(QuestStatus::REWARD);
				qs->setQuestVar(var + 1);
			}
		} else {
			if (var >= startVar && var < javaSub(endVar, 1)) {
				changeQuestStep(env, var, var + 1, false);
				return true;
			} else if (var == javaSub(endVar, 1)) {
				if (reward)
					qs->setStatus(QuestStatus::REWARD);
				else
					qs->setQuestVarById(0, var + 1);
			}
		}
		updateQuestStatus(env);
		return true;
	}
	return false;
}

bool AbstractQuestHandler::defaultOnKillInZoneEvent(model::QuestEnv& env, int32_t startVar, int32_t endVar, bool reward) {
	return defaultOnKillRankedEvent(env, startVar, endVar, reward, false);
}

bool AbstractQuestHandler::defaultOnKillInZoneEvent(model::QuestEnv& env, int32_t startVar, int32_t endVar, bool reward, bool isDataDriven) {
	return defaultOnKillRankedEvent(env, startVar, endVar, reward, isDataDriven);
}

bool AbstractQuestHandler::defaultOnUseSkillEvent(model::QuestEnv& env, int32_t startVar, int32_t endVar, int32_t varNum) {
	runtime::Ptr<Player> player = env.getPlayer();
	runtime::Ptr<QuestState> qs = player->getQuestStateList()->getQuestState(questId);
	if (qs && qs->getStatus() == QuestStatus::START) {
		int32_t var = qs->getQuestVarById(varNum);
		if (var >= startVar && var < endVar) {
			changeQuestStep(env, var, var + 1, false, varNum);
			return true;
		}
	}
	return false;
}

bool AbstractQuestHandler::defaultStartFollowEvent(model::QuestEnv& env, gameserver::model::gameobjects::Npc& follower, int32_t targetNpcId,
	int32_t step, int32_t nextStep) {
	runtime::Ptr<Player> player = env.getPlayer();
	if (!runtime::as<Npc>(env.getVisibleObject())) {
		return false;
	}
	follower.overrideNpcType(gameserver::model::CreatureType::PEACE);
	follower.getAi().onCreatureEvent(gameserver::ai::event::AIEventType::FOLLOW_ME, *player);
	player->getController().addTask(gameserver::model::TaskId::QUEST_FOLLOW,
		task::QuestTasks::newFollowingToTargetCheckTask(env, follower, targetNpcId));
	return (step == 0 && nextStep == 0) || defaultCloseDialog(env, step, nextStep);
}

bool AbstractQuestHandler::defaultStartFollowEvent(model::QuestEnv& env, gameserver::model::gameobjects::Npc& follower, float x, float y, float z,
	int32_t step, int32_t nextStep) {
	const runtime::Ptr<Player> player = env.getPlayer();
	if (!runtime::as<Npc>(env.getVisibleObject())) {
		return false;
	}
	PacketSendUtility::sendPacket(*player, network::aion::serverpackets::SM_NPC_INFO(follower, *player));
	follower.getAi().onCreatureEvent(gameserver::ai::event::AIEventType::FOLLOW_ME, *player);
	player->getController().addTask(gameserver::model::TaskId::QUEST_FOLLOW,
		task::QuestTasks::newFollowingToTargetCheckTask(env, follower, x, y, z));
	if (step == 0 && nextStep == 0) {
		return true;
	} else {
		return defaultCloseDialog(env, step, nextStep);
	}
}

bool AbstractQuestHandler::defaultFollowEndEvent(model::QuestEnv& env, int32_t step, int32_t nextStep, bool reward, int32_t movie) {
	runtime::Ptr<Player> player = env.getPlayer();
	runtime::Ptr<QuestState> qs = player->getQuestStateList()->getQuestState(questId);
	if (qs && qs->getStatus() == QuestStatus::START) {
		if (qs->getQuestVarById(0) == step) {
			changeQuestStep(env, step, nextStep, reward);
			if (movie != 0)
				playQuestMovie(env, movie);
			return true;
		}
	}
	return false;
}

bool AbstractQuestHandler::defaultFollowEndEvent(model::QuestEnv& env, int32_t step, int32_t nextStep, bool reward) {
	return defaultFollowEndEvent(env, step, nextStep, reward, 0);
}

bool AbstractQuestHandler::defaultOnGetItemEvent(model::QuestEnv& env, int32_t step, int32_t nextStep, bool reward) {
	runtime::Ptr<Player> player = env.getPlayer();
	runtime::Ptr<QuestState> qs = player->getQuestStateList()->getQuestState(questId);
	if (qs && qs->getStatus() == QuestStatus::START) {
		if (qs->getQuestVarById(0) == step) {
			changeQuestStep(env, step, nextStep, reward);
			return true;
		}
	}
	return false;
}

bool AbstractQuestHandler::useQuestObject(model::QuestEnv& env, int32_t step, int32_t nextStep, bool reward, bool die) {
	return useQuestObject(env, step, nextStep, reward, 0, 0, 0, 0, 0, 0, die);
}

bool AbstractQuestHandler::useQuestObject(model::QuestEnv& env, int32_t step, int32_t nextStep, bool reward, int32_t varNum, bool die) {
	return useQuestObject(env, step, nextStep, reward, varNum, 0, 0, 0, 0, 0, die);
}

bool AbstractQuestHandler::useQuestObject(model::QuestEnv& env, int32_t step, int32_t nextStep, bool reward, int32_t varNum) {
	return useQuestObject(env, step, nextStep, reward, varNum, 0, 0, 0, 0, 0, false);
}

bool AbstractQuestHandler::useQuestObject(model::QuestEnv& env, int32_t step, int32_t nextStep, bool reward, int32_t varNum, int32_t addItemId,
	int32_t addItemCount) {
	return useQuestObject(env, step, nextStep, reward, varNum, addItemId, addItemCount, 0, 0, 0, false);
}

bool AbstractQuestHandler::useQuestObject(model::QuestEnv& env, int32_t step, int32_t nextStep, bool reward, int32_t varNum, int32_t addItemId,
	int32_t addItemCount, int32_t removeItemId, int32_t removeItemCount) {
	return useQuestObject(env, step, nextStep, reward, varNum, addItemId, addItemCount, removeItemId, removeItemCount, 0, false);
}

bool AbstractQuestHandler::useQuestObject(model::QuestEnv& env, int32_t step, int32_t nextStep, bool reward, int32_t varNum, int32_t movieId) {
	return useQuestObject(env, step, nextStep, reward, varNum, 0, 0, 0, 0, movieId, false);
}

bool AbstractQuestHandler::useQuestObject(model::QuestEnv& env, int32_t step, int32_t nextStep, bool reward, int32_t varNum, int32_t addItemId,
	int32_t addItemCount, int32_t removeItemId, int32_t removeItemCount, int32_t movieId, bool dieObject) {
	runtime::Ptr<Player> player = env.getPlayer();
	runtime::Ptr<QuestState> qs = player->getQuestStateList()->getQuestState(questId);
	if (!qs) {
		return false;
	}
	if (qs->getQuestVarById(varNum) == step) {
		if (addItemId != 0 && addItemCount != 0) {
			if (!giveQuestItem(env, addItemId, addItemCount)) {
				return false;
			}
		}
		if (removeItemId != 0 && removeItemCount != 0) {
			removeQuestItem(env, removeItemId, removeItemCount);
		}
		if (movieId != 0) {
			playQuestMovie(env, movieId);
		}
		if (dieObject) {
			runtime::Ptr<Npc> npc = runtime::cast<Npc>(player->getTarget()); // Java: (Npc) player.getTarget()
			VisibleObject& visibleObject = *env.getVisibleObject(); // dereferenced before equals, also for a null target
			if (!npc || !visibleObject.equals(*npc)) // Java: equals(null) is false
				return false;
			npc->getController().die(*player);
		}
		changeQuestStep(env, step, nextStep, reward, varNum);
		return true;
	}
	return false;
}

bool AbstractQuestHandler::useQuestItem(model::QuestEnv& env, gameserver::model::gameobjects::Item& item, int32_t step, int32_t nextStep,
	bool reward) {
	return useQuestItem(env, item, step, nextStep, reward, 0, 0, 0);
}

bool AbstractQuestHandler::useQuestItem(model::QuestEnv& env, gameserver::model::gameobjects::Item& item, int32_t step, int32_t nextStep, bool reward,
	int32_t addItemId, int32_t addItemCount) {
	return useQuestItem(env, item, step, nextStep, reward, addItemId, addItemCount, 0);
}

bool AbstractQuestHandler::useQuestItem(model::QuestEnv& env, gameserver::model::gameobjects::Item& item, int32_t step, int32_t nextStep, bool reward,
	int32_t movieId) {
	return useQuestItem(env, item, step, nextStep, reward, 0, 0, movieId);
}

bool AbstractQuestHandler::useQuestItem(model::QuestEnv& env, gameserver::model::gameobjects::Item& item, int32_t step, int32_t nextStep, bool reward,
	int32_t addItemId, int32_t addItemCount, int32_t movieId) {
	return useQuestItem(env, item, step, nextStep, reward, addItemId, addItemCount, movieId, 0);
}

// anonymous Runnable at AbstractQuestHandler.java:956 (fieldmap.py --class 'com.aionemu.gameserver.questEngine.handlers.AbstractQuestHandler$1'):
// AbstractQuestHandler_Runnable, scheduled for 3000 ms
bool AbstractQuestHandler::useQuestItem(model::QuestEnv& env, gameserver::model::gameobjects::Item& item, int32_t step, int32_t nextStep, bool reward,
	int32_t addItemId, int32_t addItemCount, int32_t movieId, int32_t varNum) {
	runtime::Ptr<Player> playerPtr = env.getPlayer();
	if (!playerPtr) {
		return false;
	}
	Player& player = *playerPtr;
	runtime::Ptr<QuestState> qs = player.getQuestStateList()->getQuestState(questId);
	if (!qs) {
		return false;
	}
	const int32_t itemId = item.getItemId();
	const int32_t objectId = item.getObjectId();

	if (qs->getQuestVarById(varNum) == step) {
		PacketSendUtility::broadcastPacket(player, SM_ITEM_USAGE_ANIMATION(player.getObjectId(), objectId, itemId, 3000, 0, 0), true);
		// AbstractQuestHandler$1 (fieldmap K3): the task pins the player and the env it captured; the handler itself is Immortal (RT-11)
		utils::ThreadPoolManager::getInstance().schedule({this, &player, &env},
			[this, &player, &env, itemId, objectId, addItemId, addItemCount, movieId, step, nextStep, reward, varNum] {
				PacketSendUtility::broadcastPacket(player, SM_ITEM_USAGE_ANIMATION(player.getObjectId(), objectId, itemId, 0, 1, 0), true);
				removeQuestItem(env, itemId, 1);

				if (addItemId != 0 && addItemCount != 0) {
					if (!giveQuestItem(env, addItemId, addItemCount)) {
						return;
					}
				}
				if (movieId != 0) {
					playQuestMovie(env, movieId);
				}
				changeQuestStep(env, step, nextStep, reward, varNum);
			},
			3000);
		return true;
	}
	return false;
}

bool AbstractQuestHandler::defaultOnLevelChangedEvent(gameserver::model::gameobjects::player::Player& player,
	std::initializer_list<int32_t> preQuests) {
	runtime::Ptr<QuestState> qs = player.getQuestStateList()->getQuestState(questId);

	// Only null or LOCKED quests can be started
	if (qs && qs->getStatus() != QuestStatus::LOCKED)
		return false;

	const QuestTemplate& template_ = questTemplateOf(questId);
	int32_t minLvlDiff = template_.isMission() ? 2 : 0;
	// Check all player requirements (but allowed diff to quest minLevel = 2)
	if (!services::QuestService::checkStartConditions(player, questId, false, minLvlDiff, false, false, template_.isMission()))
		return false;

	bool missingRequirement = false;
	for (int32_t id : preQuests) {
		runtime::Ptr<QuestState> qs2 = player.getQuestStateList()->getQuestState(id);
		if (!missingRequirement && (!qs2 || qs2->getStatus() != QuestStatus::COMPLETE)) {
			if (qs || !template_.isMission()) // fast return if its already locked or no campaign quest
				return false;
			missingRequirement = true;
		}
		if (missingRequirement && qs2 && qs2->getStatus() == QuestStatus::COMPLETE) {
			services::QuestService::addOrUpdateQuest(player, questId, QuestStatus::LOCKED);
			return false;
		}
	}
	if (missingRequirement)
		return false;

	// Check the quests, that have to be done before starting this one and other start conditions, listed in quest_data
	for (const gameserver::model::templates::quest::XMLStartCondition& cond : template_.getXMLStartConditions()) {
		if (!cond.check(player, false)) {
			if (!qs && template_.isMission())
				services::QuestService::addOrUpdateQuest(player, questId, QuestStatus::LOCKED);
			return false;
		}
	}

	// Send locked quest if the player is <= 2 levels below quest min level (as specified in the check above)
	if (minLvlDiff > 0 && player.getLevel() < template_.getMinlevelPermitted()) {
		if (!qs && template_.isMission())
			services::QuestService::addOrUpdateQuest(player, questId, QuestStatus::LOCKED);
		return false;
	}

	// All conditions are met, start the quest
	services::QuestService::addOrUpdateQuest(player, questId, QuestStatus::START);
	return true;
}

bool AbstractQuestHandler::defaultOnQuestCompletedEvent(model::QuestEnv& env, std::initializer_list<int32_t> preQuests) {
	runtime::Ptr<Player> player = env.getPlayer();
	int32_t finishedQuestId = env.getQuestId();
	runtime::Ptr<gameserver::model::gameobjects::player::QuestStateList> qsl = player->getQuestStateList();
	runtime::Ptr<QuestState> qs = qsl->getQuestState(questId);

	// Only null or LOCKED quests can be started
	if (qs && qs->getStatus() != QuestStatus::LOCKED)
		return false;

	const QuestTemplate& template_ = questTemplateOf(questId);
	int32_t minLvlDiff = template_.isMission() ? 15 : 0; // this ensures to add all follow-up quests in locked state
	// Check all player requirements first
	if (!services::QuestService::checkStartConditions(*player, questId, false, minLvlDiff, false, false, template_.isMission()))
		return false;

	bool missingRequirement = false;
	bool hasFinishedPreQuest = false;
	for (int32_t id : preQuests) {
		runtime::Ptr<QuestState> qs2 = qsl->getQuestState(id);
		if (!missingRequirement && (!qs2 || qs2->getStatus() != QuestStatus::COMPLETE)) {
			if (qs || !template_.isMission()) // fast return if its already locked or no campaign quest
				return false;
			missingRequirement = true;
		}
		if (finishedQuestId == id)
			hasFinishedPreQuest = true;
		if (missingRequirement && (hasFinishedPreQuest || (qs2 && qs2->getStatus() == QuestStatus::COMPLETE))) { // if any pre quest is finished
			services::QuestService::addOrUpdateQuest(*player, questId, QuestStatus::LOCKED);
			return false;
		}
	}
	if (missingRequirement)
		return false;

	// Check the quests, that have to be done before starting this one and other start conditions, listed in quest_data
	missingRequirement = false;
	for (const gameserver::model::templates::quest::XMLStartCondition& cond : template_.getXMLStartConditions()) {
		if (!cond.check(*player, false)) {
			if (qs || !template_.isMission()) // fast return if its already locked or no campaign quest
				return false;
			else if (hasAnyPreQuestFinished(*qsl, &cond)) { // recursive check
				services::QuestService::addOrUpdateQuest(*player, questId, QuestStatus::LOCKED);
				return false;
			}
			missingRequirement = true;
		}
	}
	if (missingRequirement)
		return false;

	// Send locked quest if the players level is in the minLvlDiff range (1-15)
	if (minLvlDiff > 0 && player->getLevel() < template_.getMinlevelPermitted()) {
		if (!qs && hasFinishedPreQuest)
			services::QuestService::addOrUpdateQuest(*player, questId, QuestStatus::LOCKED);
		return false;
	}

	// All conditions are met, start the quest
	services::QuestService::addOrUpdateQuest(*player, questId, QuestStatus::START);
	return true;
}

bool AbstractQuestHandler::hasAnyPreQuestFinished(gameserver::model::gameobjects::player::QuestStateList& qsl,
	const gameserver::model::templates::quest::XMLStartCondition* startCondition) {
	if (startCondition == nullptr)
		throw runtime::NullPointerException("startCondition");
	// Java: a null list without <finished>; C++: an empty one
	for (const gameserver::model::templates::quest::FinishedQuestCond& finishedCond : startCondition->getFinishedPreconditions()) {
		runtime::Ptr<QuestState> qs = qsl.getQuestState(finishedCond.getQuestId());
		if (qs && qs->getStatus() == QuestStatus::COMPLETE)
			return true;
		const QuestTemplate& template_ = questTemplateOf(finishedCond.getQuestId());
		for (const gameserver::model::templates::quest::XMLStartCondition& cond : template_.getXMLStartConditions())
			if (hasAnyPreQuestFinished(qsl, &cond))
				return true;
	}
	return false;
}

bool AbstractQuestHandler::defaultOnEnterZoneEvent(model::QuestEnv& env, const world::zone::ZoneName* currentZoneName,
	const world::zone::ZoneName* questZoneName) {
	if (questZoneName == currentZoneName) {
		runtime::Ptr<Player> player = env.getPlayer();
		if (!player)
			return false;
		runtime::Ptr<QuestState> qs = player->getQuestStateList()->getQuestState(questId);
		if (!qs) {
			env.setQuestId(questId);
			if (services::QuestService::startQuest(env))
				return true;
		}
	}
	return false;
}

bool AbstractQuestHandler::sendQuestRewardDialog(model::QuestEnv& env, int32_t rewardNpcId, int32_t reportDialogId) {
	runtime::Ptr<Player> player = env.getPlayer();
	runtime::Ptr<QuestState> qs = player->getQuestStateList()->getQuestState(questId);
	if (qs->getStatus() == QuestStatus::REWARD) {
		if (env.getTargetId() == rewardNpcId) {
			if (env.getDialogActionId() == DialogAction::USE_OBJECT && reportDialogId != 0) {
				return sendQuestDialog(env, reportDialogId);
			} else {
				return sendQuestEndDialog(env);
			}
		}
	}
	return false;
}

bool AbstractQuestHandler::sendQuestNoneDialog(model::QuestEnv& env, int32_t startNpcId) {
	const QuestTemplate* template_ = dataholders::DataManager::QUEST_DATA->getQuestById(questId);
	return sendQuestNoneDialog(env, template_, startNpcId, 1011);
}

bool AbstractQuestHandler::sendQuestNoneDialog(model::QuestEnv& env, int32_t startNpcId, int32_t dialogPageId) {
	const QuestTemplate* template_ = dataholders::DataManager::QUEST_DATA->getQuestById(questId);
	return sendQuestNoneDialog(env, template_, startNpcId, dialogPageId);
}

bool AbstractQuestHandler::sendQuestNoneDialog(model::QuestEnv& env, const gameserver::model::templates::QuestTemplate* template_, int32_t startNpcId,
	int32_t dialogPageId) {
	static_cast<void>(template_); // Java: passed on by the overloads, never read
	runtime::Ptr<Player> player = env.getPlayer();
	runtime::Ptr<QuestState> qs = player->getQuestStateList()->getQuestState(questId);
	if (!qs || qs->isStartable()) {
		if (env.getTargetId() == startNpcId) {
			if (env.getDialogActionId() == DialogAction::QUEST_SELECT) {
				return sendQuestDialog(env, dialogPageId);
			} else {
				return sendQuestStartDialog(env);
			}
		}
	}
	return false;
}

bool AbstractQuestHandler::sendQuestNoneDialog(model::QuestEnv& env, int32_t startNpcId, int32_t dialogPageId, int32_t itemId, int32_t itemCout) {
	const QuestTemplate* template_ = dataholders::DataManager::QUEST_DATA->getQuestById(questId);
	return sendQuestNoneDialog(env, template_, startNpcId, dialogPageId, itemId, itemCout);
}

bool AbstractQuestHandler::sendQuestNoneDialog(model::QuestEnv& env, int32_t startNpcId, int32_t itemId, int32_t itemCout) {
	const QuestTemplate* template_ = dataholders::DataManager::QUEST_DATA->getQuestById(questId);
	return sendQuestNoneDialog(env, template_, startNpcId, 1011, itemId, itemCout);
}

bool AbstractQuestHandler::sendQuestNoneDialog(model::QuestEnv& env, const gameserver::model::templates::QuestTemplate* template_, int32_t startNpcId,
	int32_t dialogPageId, int32_t itemId, int32_t itemCout) {
	static_cast<void>(template_); // Java: passed on by the overloads, never read
	runtime::Ptr<Player> player = env.getPlayer();
	runtime::Ptr<QuestState> qs = player->getQuestStateList()->getQuestState(questId);
	if (!qs || qs->isStartable()) {
		if (env.getTargetId() == startNpcId) {
			if (env.getDialogActionId() == DialogAction::QUEST_SELECT) {
				return sendQuestDialog(env, dialogPageId);
			}
			if (itemId != 0 && itemCout != 0) {
				if (env.getDialogActionId() == DialogAction::QUEST_ACCEPT_1) {
					if (giveQuestItem(env, itemId, itemCout)) {
						return sendQuestStartDialog(env);
					} else {
						return true;
					}
				} else {
					return sendQuestStartDialog(env);
				}
			} else {
				return sendQuestStartDialog(env);
			}
		}
	}
	return false;
}

bool AbstractQuestHandler::sendItemCollectingStartDialog(model::QuestEnv& env) {
	switch (env.getDialogActionId()) {
		case DialogAction::QUEST_ACCEPT_1:
			services::QuestService::startQuest(env);
			return sendQuestSelectionDialog(env);
		case DialogAction::QUEST_REFUSE_1:
			return sendQuestSelectionDialog(env);
	}
	return false;
}

runtime::Ptr<gameserver::model::gameobjects::VisibleObject> AbstractQuestHandler::spawn(int32_t templateId,
	gameserver::model::gameobjects::VisibleObject& objectToGetInstanceFrom, float x, float y, float z, int8_t heading) {
	return spawn(templateId, *objectToGetInstanceFrom.getWorldMapInstance(), x, y, z, heading);
}

runtime::Ptr<gameserver::model::gameobjects::VisibleObject> AbstractQuestHandler::spawn(int32_t templateId, world::WorldMapInstance& worldMapInstance,
	float x, float y, float z, int8_t heading) {
	runtime::Ref<gameserver::model::templates::spawns::SpawnTemplate> template_ =
		spawnengine::SpawnEngine::newSingleTimeSpawn(worldMapInstance.getMapId(), templateId, x, y, z, heading);
	return spawnengine::SpawnEngine::spawnObject(*template_, worldMapInstance.getInstanceId());
}

runtime::Ptr<gameserver::model::gameobjects::VisibleObject> AbstractQuestHandler::spawnInFrontOf(int32_t templateId,
	gameserver::model::gameobjects::VisibleObject& referencePositionObject) {
	return spawnInFront(templateId, *referencePositionObject.getPosition(), std::nullopt, 1.5f, 0);
}

runtime::Ptr<gameserver::model::gameobjects::VisibleObject> AbstractQuestHandler::spawnForFiveMinutesInFrontOf(int32_t templateId,
	gameserver::model::gameobjects::VisibleObject& referencePositionObject, float distance) {
	return spawnInFront(templateId, *referencePositionObject.getPosition(), std::nullopt, distance, 5);
}

runtime::Ptr<gameserver::model::gameobjects::VisibleObject> AbstractQuestHandler::spawnForFiveMinutesInFront(int32_t templateId,
	gameserver::model::gameobjects::VisibleObject& referencePositionObject, int8_t heading, float distance) {
	return spawnInFront(templateId, *referencePositionObject.getPosition(), heading, distance, 5);
}

runtime::Ptr<gameserver::model::gameobjects::VisibleObject> AbstractQuestHandler::spawnInFront(int32_t templateId,
	world::WorldPosition& referencePosition, std::optional<int8_t> heading, float distance, int32_t timeInMin) {
	if (!heading) { // make the spawn face towards referencePosition
		int32_t referenceHeading = referencePosition.getHeading();
		heading = static_cast<int8_t>(referenceHeading < 60 ? referenceHeading + 60 : referenceHeading - 60);
	}
	// Java: Math.toRadians(double), angdeg * DEGREES_TO_RADIANS since JDK 9
	double radian = static_cast<double>(utils::PositionUtil::convertHeadingToAngle(referencePosition.getHeading())) * 0.017453292519943295;
	float x = referencePosition.getX() + static_cast<float>(std::cos(radian) * static_cast<double>(distance));
	float y = referencePosition.getY() + static_cast<float>(std::sin(radian) * static_cast<double>(distance));
	float z = referencePosition.getZ();
	float geoZ = world::geo::GeoService::getInstance().getZ(referencePosition.getMapId(), x, y, z + 2, z - 1, referencePosition.getInstanceId());
	if (!std::isnan(geoZ))
		z = geoZ;
	return spawnTemporarily(templateId, *referencePosition.getWorldMapInstance(), x, y, z, *heading, timeInMin);
}

runtime::Ptr<gameserver::model::gameobjects::VisibleObject> AbstractQuestHandler::spawnForFiveMinutes(int32_t templateId,
	world::WorldPosition& position) {
	return spawnForFiveMinutes(templateId, position, position.getHeading());
}

runtime::Ptr<gameserver::model::gameobjects::VisibleObject> AbstractQuestHandler::spawnForFiveMinutes(int32_t templateId,
	world::WorldPosition& position, int8_t heading) {
	return spawnTemporarily(templateId, *position.getWorldMapInstance(), position.getX(), position.getY(), position.getZ(), heading, 5);
}

runtime::Ptr<gameserver::model::gameobjects::VisibleObject> AbstractQuestHandler::spawnForFiveMinutes(int32_t templateId,
	world::WorldMapInstance& worldMapInstance, float x, float y, float z, int8_t heading) {
	return spawnTemporarily(templateId, worldMapInstance, x, y, z, heading, 5);
}

// lambda at AbstractQuestHandler.java:1287: scheduled deleteIfAliveOrCancelRespawn, pin {&object}
runtime::Ptr<gameserver::model::gameobjects::VisibleObject> AbstractQuestHandler::spawnTemporarily(int32_t templateId,
	world::WorldMapInstance& worldMapInstance, float x, float y, float z, int8_t heading, int32_t timeInMin) {
	runtime::Ptr<VisibleObject> object = spawn(templateId, worldMapInstance, x, y, z, heading);
	if (timeInMin > 0) {
		int32_t delay = javaMul(60000, timeInMin); // Java: int 60000 * timeInMin, widened to the long delay after the int product
		if (object) {
			VisibleObject& spawned = *object;
			utils::ThreadPoolManager::getInstance().schedule({&spawned}, [&spawned] { spawned.getController().deleteIfAliveOrCancelRespawn(); }, delay);
		} else {
			// Java: the lambda dereferences the null spawn when it runs (a NullPointerException the RunnableWrapper logs)
			utils::ThreadPoolManager::getInstance().schedule(
				[] { throw runtime::NullPointerException("AbstractQuestHandler.spawnTemporarily: the spawn failed (object is null)"); }, delay);
		}
	}
	return object;
}

} // namespace aion::gameserver::questEngine::handlers
