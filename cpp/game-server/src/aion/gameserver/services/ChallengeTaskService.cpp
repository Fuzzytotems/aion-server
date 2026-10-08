#include "aion/gameserver/services/ChallengeTaskService.h"

#include <map>
#include <optional>
#include <string>
#include <unordered_map>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/configs/main/CustomConfig.h"
#include "aion/gameserver/dao/ChallengeTasksDAO.h"
#include "aion/gameserver/dao/TownDAO.h"
#include "aion/gameserver/dataholders/ChallengeData.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/model/challenge/ChallengeQuest.h"
#include "aion/gameserver/model/challenge/ChallengeTask.h"
#include "aion/gameserver/model/gameobjects/LetterType.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/team/legion/Legion.h"
#include "aion/gameserver/model/team/legion/LegionMember.h"
#include "aion/gameserver/model/templates/challenge/ChallengeReward.h"
#include "aion/gameserver/model/templates/challenge/ChallengeTaskTemplate.h"
#include "aion/gameserver/model/templates/challenge/ContributionReward.h"
#include "aion/gameserver/model/town/Town.h"
#include "aion/gameserver/network/aion/serverpackets/SM_CHALLENGE_LIST.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/services/LegionService.h"
#include "aion/gameserver/services/TownService.h"
#include "aion/gameserver/services/mail/SystemMailService.h"
#include "aion/gameserver/services/player/PlayerService.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/utils/PacketSendUtility.h"

namespace aion::gameserver::services {

using model::challenge::ChallengeQuest;
using model::challenge::ChallengeTask;
using model::gameobjects::player::Player;
using model::templates::challenge::ChallengeTaskTemplate;
using model::templates::challenge::ChallengeType;
using network::aion::serverpackets::SM_CHALLENGE_LIST;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;
using TaskMap = runtime::RcHashMap<int32_t, runtime::Ref<ChallengeTask>>;
using TaskMaps = runtime::ConcurrentHashMap<int32_t, runtime::Ref<TaskMap>>;

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.services.ChallengeTaskService");

namespace {
/** Java: the HashMap ChallengeTasksDAO.load returns - the tasks keyed by task id, in a map of their own */
runtime::Ref<TaskMap> loadTasks(int32_t ownerId, ChallengeType challengeType) {
	runtime::Ref<TaskMap> tasks = TaskMap::create(AION_LOCK_CLASS(ChallengeTaskService::tasks));
	for (auto& [taskId, task] : dao::ChallengeTasksDAO::load(ownerId, challengeType))
		tasks->put(taskId, std::move(task));
	return tasks;
}
} // namespace

ChallengeTaskService::ChallengeTaskService() {
	log.info("ChallengeTaskService initialized.");
}

ChallengeTaskService::~ChallengeTaskService() = default;

ChallengeTaskService& ChallengeTaskService::getInstance() {
	static ChallengeTaskService instance; // Java SingletonHolder
	return instance;
}

void ChallengeTaskService::showTaskList(Player& player, ChallengeType challengeType, int32_t ownerId) {
	if (configs::main::CustomConfig::CHALLENGE_TASKS_ENABLED.load()) {
		int32_t ownerLevel = 0;
		switch (challengeType) {
			case ChallengeType::TOWN:
				ownerLevel = TownService::getInstance().getTownById(ownerId)->getLevel();
				break;
			case ChallengeType::LEGION:
				ownerLevel = player.getLegion()->getLegionLevel();
				break;
		}
		std::vector<runtime::Ptr<ChallengeTask>> availableTasks = buildTaskList(player, challengeType, ownerId, ownerLevel);
		utils::PacketSendUtility::sendPacket(player, SM_CHALLENGE_LIST(2, ownerId, challengeType, availableTasks));
		for (const runtime::Ptr<ChallengeTask>& task : availableTasks) {
			utils::PacketSendUtility::sendPacket(player, SM_CHALLENGE_LIST(7, ownerId, challengeType, *task));
		}
	}
}

std::vector<runtime::Ptr<ChallengeTask>> ChallengeTaskService::buildTaskList(Player& player, ChallengeType challengeType, int32_t ownerId,
	int32_t ownerLevel) {
	TaskMaps* taskMap;
	if (challengeType == ChallengeType::LEGION)
		taskMap = &legionTasks;
	else
		taskMap = &cityTasks;
	std::vector<runtime::Ptr<ChallengeTask>> availableTasks;
	if (!taskMap->containsKey(ownerId)) {
		runtime::Ref<TaskMap> tasks = loadTasks(ownerId, challengeType);
		taskMap->put(ownerId, std::move(tasks));
	}
	for (const runtime::Ref<ChallengeTask>& ct : taskMap->get(ownerId)->values()) {
		if (ct->getTemplate()->isRepeatable() || !ct->isCompleted())
			availableTasks.push_back(ct);
	}
	for (const auto& [id, template_] : dataholders::DataManager::CHALLENGE_DATA->getTasks()) {
		if (template_->getType() == challengeType && template_->getRace() == player.getRace()) {
			if (!taskMap->get(ownerId)->containsKey(template_->getId())) {
				if (ownerLevel >= template_->getMinLevel() && ownerLevel <= template_->getMaxLevel()) {
					if (!template_->getPrevTask()) {
						runtime::Ref<ChallengeTask> task = ChallengeTask::create(ownerId, template_);
						taskMap->get(ownerId)->put(task->getTaskId(), task);
						dao::ChallengeTasksDAO::storeTask(*task);
						availableTasks.push_back(task);
					} else {
						int32_t prevTaskId = *template_->getPrevTask();
						if (taskMap->get(ownerId)->containsKey(prevTaskId)) {
							runtime::Ptr<ChallengeTask> prevTask = *taskMap->get(ownerId)->get(prevTaskId);
							if (prevTask->isCompleted()) {
								runtime::Ref<ChallengeTask> task = ChallengeTask::create(ownerId, template_);
								taskMap->get(ownerId)->put(task->getTaskId(), task);
								dao::ChallengeTasksDAO::storeTask(*task);
								availableTasks.push_back(task);
							}
						}
					}
				}
			}
		}
	}
	return availableTasks;
}

void ChallengeTaskService::onChallengeQuestFinish(Player& player, int32_t questId) {
	const ChallengeTaskTemplate* taskTemplate = dataholders::DataManager::CHALLENGE_DATA->getTaskByQuestId(questId);
	switch (taskTemplate->getType()) {
		case ChallengeType::TOWN:
			onCityTaskFinish(player, taskTemplate, questId);
			break;
		case ChallengeType::LEGION:
			onLegionTaskFinish(player, taskTemplate, questId);
			break;
	}
}

void ChallengeTaskService::onAcceptTask(Player& player, int32_t questId) {
	int32_t townId = TownService::getInstance().getTownIdByPosition(player);
	if (townId != 0)
		taskAcceptTownIds.computeIfAbsent(player.getObjectId(), [] { return runtime::RcHashMap<int32_t, int32_t>::create(); })->put(questId, townId);
}

void ChallengeTaskService::onCityTaskFinish(Player& player, const ChallengeTaskTemplate* taskTemplate, int32_t questId) {
	runtime::Ptr<runtime::RcHashMap<int32_t, int32_t>> townsByQuestId = taskAcceptTownIds.get(player.getObjectId());
	std::optional<int32_t> townId = !townsByQuestId ? std::nullopt : townsByQuestId->remove(questId);
	if (!townId) // server got restarted after player accepted the quest or quest got started outside town (by chat command)
		return;
	runtime::Ptr<ChallengeTask> task = getChallengeTask(player, taskTemplate, *townId);
	if (!task) // task may be not available anymore due to town levelup
		return;
	runtime::Ptr<ChallengeQuest> quest = task->getQuest(questId);
	if (!quest) {
		log.warn(player.toString() + " finished city task " + std::to_string(task->getTaskId()) + " of town " + std::to_string(*townId) +
			" but info for quest " + std::to_string(questId) + " is missing.");
		return;
	}
	if (quest->getCompleteCount() < quest->getMaxRepeats() && !task->isCompleted()) {
		task->updateCompleteTime();
		quest->increaseCompleteCount();
		dao::ChallengeTasksDAO::storeTask(*task);
		runtime::Ptr<model::town::Town> town = TownService::getInstance().getTownById(*townId);
		if (town) {
			int32_t oldLevel = town->getLevel();
			town->increasePoints(quest->getScorePerQuest());
			if (task->isCompleted()) {
				switch (taskTemplate->getReward()->getType()) {
					case model::templates::challenge::RewardType::POINT:
						town->increasePoints(*taskTemplate->getReward()->getValue());
						break;
					case model::templates::challenge::RewardType::SPAWN:
						// TODO
						break;
				}
				utils::PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_TOWN_MISSION_COMPLETE(town->getL10n(), task->getTemplate()->getL10n()));
			}
			if (town->getLevel() != oldLevel)
				utils::PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_TOWN_LEVEL_LEVEL_UP(town->getL10n(), town->getLevel()));
			dao::TownDAO::store(*town);
		}
	}
}

runtime::Ptr<ChallengeTask> ChallengeTaskService::getChallengeTask(Player& player, const ChallengeTaskTemplate* taskTemplate, int32_t townId) {
	runtime::Ptr<TaskMap> taskMap = cityTasks.get(townId);
	if (!taskMap) {
		buildTaskList(player, ChallengeType::TOWN, townId, TownService::getInstance().getTownById(townId)->getLevel());
		taskMap = cityTasks.get(townId);
		if (!taskMap) {
			log.warn("Town " + std::to_string(townId) + " has no CityTasks! " + player.toString() + ", town residence:" +
				std::to_string(TownService::getInstance().getTownResidence(player)));
			return nullptr;
		}
	}
	return taskMap->get(taskTemplate->getId());
}

void ChallengeTaskService::onLegionTaskFinish(Player& player, const ChallengeTaskTemplate* taskTemplate, int32_t questId) {
	// Player could take challenge task and after that leave legion.
	if (!player.getLegion())
		return;
	int32_t legionId = player.getLegion()->getLegionId();
	// If player took challenge task in one legion, then leave that legion and enter another.
	if (!legionTasks.containsKey(legionId))
		return;
	// If player took challenge task in one legion, then leave that legion and enter another, and after that completed this task in new legion.
	if (!legionTasks.get(legionId)->get(taskTemplate->getId()))
		return;
	runtime::Ptr<ChallengeTask> task = legionTasks.get(player.getLegion()->getLegionId())->get(taskTemplate->getId());
	runtime::Ptr<ChallengeQuest> quest = task->getQuests().get(questId);
	if (quest->getCompleteCount() >= quest->getMaxRepeats())
		return;
	player.getLegionMember()->increaseChallengeScore(quest->getScorePerQuest());
	if (!task->isCompleted()) {
		task->updateCompleteTime();
		quest->increaseCompleteCount();
		for (const runtime::Ptr<Player>& p : player.getLegion()->getOnlinePlayers())
			showTaskList(*p, ChallengeType::LEGION, legionId);
		dao::ChallengeTasksDAO::storeTask(*task);
		if (task->isCompleted()) {
			// Java: TreeMap<Integer, List<Integer>> winnersByPoints, read in descending key order
			std::map<int32_t, std::vector<int32_t>> winnersByPoints;
			for (const runtime::Ptr<model::team::legion::LegionMember>& legionMember : player.getLegion()->getMembers()) {
				winnersByPoints[legionMember->getChallengeScore()].push_back(legionMember->getObjectId());
				legionMember->setChallengeScore(0);
				if (!legionMember->isOnline()) // save legionMember to DB since owning player is not online (no autosave schedule)
					LegionService::getInstance().storeLegionMember(*legionMember);
			}
			int32_t rewardsAdded = 0, itemId, itemCount;
			for (auto e = winnersByPoints.rbegin(); e != winnersByPoints.rend(); ++e) {
				for (int32_t objectId : e->second) {
					for (const model::templates::challenge::ContributionReward& reward : taskTemplate->getContrib()) {
						if (rewardsAdded <= reward.getNumber()) {
							rewardsAdded++;
							itemId = reward.getRewardId();
							itemCount = reward.getItemCount();
							// Java: String recipientName = PlayerService.getPlayerName(objectId) - null for an unknown id, which sendMail refuses
							std::optional<std::string> recipientName = player::PlayerService::getPlayerName(objectId);
							if (!recipientName) // Java: sendMail reads recipientName.length() (SystemMailService.java:52)
								throw runtime::NullPointerException("recipientName");
							mail::SystemMailService::sendMail("Legion reward", *recipientName, "", "", itemId, itemCount, 0,
								model::gameobjects::LetterType::NORMAL);
							break;
						}
					}
				}
				e->second.clear();
			}
		}
	}
}

bool ChallengeTaskService::canRaiseLegionLevel(model::team::legion::Legion& legion, Player& actingPlayer) {
	runtime::Ptr<TaskMap> tasks = legionTasks.computeIfAbsent(legion.getLegionId(), [&legion] { return loadTasks(legion.getLegionId(), ChallengeType::LEGION); });
	std::vector<runtime::Ptr<ChallengeTask>> requiredTasksForLevel;
	for (const runtime::Ref<ChallengeTask>& task : tasks->values()) {
		const ChallengeTaskTemplate* taskTemplate = task->getTemplate();
		if (taskTemplate->isLegionLevelTask() && taskTemplate->getMinLevel() == legion.getLegionLevel())
			requiredTasksForLevel.push_back(task);
	}
	if (requiredTasksForLevel.empty()) {
		log.warn(actingPlayer.toString() + " tried to increase level of " + legion.toString() + " but no challenge tasks were found");
		return false;
	}
	for (const runtime::Ptr<ChallengeTask>& task : requiredTasksForLevel)
		if (!task->isCompleted())
			return false;
	return true;
}

} // namespace aion::gameserver::services
