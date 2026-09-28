#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"

#include <algorithm>
#include <optional>

#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/controllers/PlayerController.h"
#include "aion/gameserver/dao/PlayerQuestListDAO.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/PlayerExperienceTable.h"
#include "aion/gameserver/model/GenderInfo.h"
#include "aion/gameserver/model/PlayerClassInfo.h"
#include "aion/gameserver/model/RaceInfo.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/QuestStateList.h"
#include "aion/gameserver/model/gameobjects/player/RatesInfo.h"
#include "aion/gameserver/model/gameobjects/player/detail/PlayerMath.h"
#include "aion/gameserver/model/stats/calc/Stat2.h"
#include "aion/gameserver/model/stats/container/PlayerGameStats.h"
#include "aion/gameserver/network/aion/serverpackets/SM_DP_INFO.h"
#include "aion/gameserver/network/aion/serverpackets/SM_STATUPDATE_DP.h"
#include "aion/gameserver/network/aion/serverpackets/SM_STATUPDATE_EXP.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/questEngine/model/QuestState.h"
#include "aion/gameserver/questEngine/model/QuestStatus.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/world/World.h"

namespace aion::gameserver::model::gameobjects::player {

namespace {

/** Java DataManager.PLAYER_EXPERIENCE_TABLE (NullPointerException while it is not published) */
const dataholders::PlayerExperienceTable& experienceTable() {
	return *dataholders::DataManager::PLAYER_EXPERIENCE_TABLE;
}

using detail::getExpLoss;
using detail::javaRound;
using detail::toInt;
using detail::toLong;

} // namespace

PlayerCommonData::PlayerCommonData(int32_t objId) : playerObjId(objId) {
}

PlayerCommonData::~PlayerCommonData() = default;

runtime::Ref<PlayerCommonData> PlayerCommonData::create(int32_t objId) {
	return runtime::makeRef<PlayerCommonData>(objId);
}

int64_t PlayerCommonData::getExpShown() {
	return exp.get() - experienceTable().getStartExpForLevel(getLevel());
}

int64_t PlayerCommonData::getExpNeed() {
	if (getLevel() == experienceTable().getMaxLevel())
		return 0;
	return experienceTable().getStartExpForLevel(getLevel() + 1) - experienceTable().getStartExpForLevel(getLevel());
}

void PlayerCommonData::calculateExpLoss() {
	int64_t expLost = getExpLoss(getLevel(), getExpNeed());

	int32_t unrecoverable = toInt(static_cast<double>(expLost) * 0.33333333);
	int32_t recoverable = static_cast<int32_t>(expLost) - unrecoverable;
	int64_t allExpLost = recoverable + expRecoverable.get();

	if (getExpShown() > unrecoverable)
		exp.set(exp.get() - unrecoverable);
	else
		exp.set(exp.get() - getExpShown());
	if (getExpShown() > recoverable) {
		expRecoverable.set(allExpLost);
		exp.set(exp.get() - recoverable);
	} else {
		expRecoverable.set(expRecoverable.get() + getExpShown());
		exp.set(exp.get() - getExpShown());
	}
	if (static_cast<double>(expRecoverable.get()) > static_cast<double>(getExpNeed()) * 0.25)
		expRecoverable.set(javaRound(static_cast<double>(getExpNeed()) * 0.25));
	if (getPlayer()) {
		utils::PacketSendUtility::sendPacket(*getPlayer(), network::aion::serverpackets::SM_STATUPDATE_EXP(getExpShown(), getExpRecoverable(),
																getExpNeed(), getCurrentReposeEnergy(), getMaxReposeEnergy()));
	}
}

void PlayerCommonData::resetRecoverableExp() {
	int64_t el = expRecoverable.get();
	expRecoverable.set(0);
	setExp(exp.get() + el);
}

void PlayerCommonData::addExp(int64_t value, Rates rates) {
	addExp(value, rates, std::nullopt);
}

void PlayerCommonData::addExp(int64_t value, Rates rates, std::optional<std::string_view> nameValue) {
	// java-race: unsynchronized read-modify-write of exp, repose and salvation energy (lost experience when two rewards race)
	if (noExp.get())
		return;

	int64_t reward = value;
	int64_t repose = 0;
	int64_t salvation = 0;
	runtime::Ptr<Player> player = getPlayer();
	if (player && player->getWorldId() == 301200000) // nightmare circus
		return;

	if (player)
		reward = calcResult(rates, *player, value);

	if (reward > 0) {
		if (getCurrentReposeEnergy() > 0) {
			int64_t allowedExp = std::min(getCurrentReposeEnergy(), reward);
			addReposeEnergy(-allowedExp);
			repose = toLong((static_cast<float>(allowedExp) / 100.0f) * 40); // 40% bonus for the amount of used repose energy
		}

		if (isReadyForSalvationPoints() && getCurrentSalvationPercent() > 0) {
			salvation = toLong((static_cast<float>(reward) / 100.0f) * getCurrentSalvationPercent());
			// TODO! remove salvation points?
		}

		reward += repose + salvation;
	}

	setExp(exp.get() + reward);
	if (player) {
		using network::aion::serverpackets::SM_SYSTEM_MESSAGE;
		if (repose > 0 && salvation > 0) {
			if (nameValue) // You have gained %num1 XP from %0 (Energy of Repose %num2, Energy of Salvation %num3).
				utils::PacketSendUtility::sendPacket(*player, SM_SYSTEM_MESSAGE::STR_GET_EXP_VITAL_MAKEUP_BONUS(*nameValue, reward, repose, salvation));
			else // You have gained %num1 XP(Energy of Repose %num2, Energy of Salvation %num3).
				utils::PacketSendUtility::sendPacket(*player, SM_SYSTEM_MESSAGE::STR_GET_EXP2_VITAL_MAKEUP_BONUS(reward, repose, salvation));
		} else if (repose > 0 && salvation == 0) {
			if (nameValue) // You have gained %num1 XP from %0 (Energy of Repose %num2).
				utils::PacketSendUtility::sendPacket(*player, SM_SYSTEM_MESSAGE::STR_GET_EXP_VITAL_BONUS(*nameValue, reward, repose));
			else // You have gained %num1 XP(Energy of Repose %num2).
				utils::PacketSendUtility::sendPacket(*player, SM_SYSTEM_MESSAGE::STR_GET_EXP2_VITAL_BONUS(reward, repose));
		} else if (repose == 0 && salvation > 0) {
			if (nameValue) // You have gained %num1 XP from %0 (Energy of Salvation %num2).
				utils::PacketSendUtility::sendPacket(*player, SM_SYSTEM_MESSAGE::STR_GET_EXP_MAKEUP_BONUS(*nameValue, reward, salvation));
			else // You have gained %num1 XP (Energy of Salvation %num2).
				utils::PacketSendUtility::sendPacket(*player, SM_SYSTEM_MESSAGE::STR_GET_EXP2_MAKEUP_BONUS(reward, salvation));
		} else {
			if (nameValue) // You have gained %num1 XP from %0.
				utils::PacketSendUtility::sendPacket(*player, SM_SYSTEM_MESSAGE::STR_GET_EXP(*nameValue, reward));
			else // You have gained %num1 XP.
				utils::PacketSendUtility::sendPacket(*player, SM_SYSTEM_MESSAGE::STR_GET_EXP2(reward));
		}
		if (getLevel() == 9 && exp.get() >= experienceTable().getStartExpForLevel(10))
			utils::PacketSendUtility::sendPacket(*player, SM_SYSTEM_MESSAGE::STR_LEVEL_LIMIT_QUEST_NOT_FINISHED1());
	}
}

bool PlayerCommonData::isReadyForSalvationPoints() {
	return getLevel() >= 15;
}

bool PlayerCommonData::isReadyForReposeEnergy() {
	return getLevel() >= 10;
}

void PlayerCommonData::addReposeEnergy(int64_t add) {
	// java-race: unsynchronized read-modify-write of the repose energy
	reposeCurrent.set(reposeCurrent.get() + add);
	if (reposeCurrent.get() < 0)
		reposeCurrent.set(0);
	else if (reposeCurrent.get() > getMaxReposeEnergy())
		reposeCurrent.set(getMaxReposeEnergy());
}

void PlayerCommonData::updateMaxRepose() {
	if (!isReadyForReposeEnergy()) {
		reposeCurrent.set(0);
		reposeMax.set(0);
	} else {
		reposeMax.set(toLong(static_cast<float>(getExpNeed()) * 0.25f)); // Retail 99%
		reposeCurrent.set(std::min(reposeMax.get(), reposeCurrent.get()));
	}
}

void PlayerCommonData::setExp(int64_t expValue) {
	// java-race: the level is computed from exp and stored without a lock; concurrent addExp calls can store a level of a stale exp value
	if (expValue != exp.get() || (level.get() == 0 && expValue == 0)) {
		bool daevaLevels = isDaeva_.get() || (!online.get() && (updateDaeva() || expValue > experienceTable().getStartExpForLevel(10)));
		int32_t maxLevel = daevaLevels ? experienceTable().getMaxLevel() : 10;
		int32_t oldLevel = level.get();

		exp.set(std::min(expValue, experienceTable().getStartExpForLevel(maxLevel)));
		// maxLevel is 66 (10 for non daeva) but 65 (9 for non daeva) should be shown with full XP bar
		level.set(std::min(experienceTable().getLevelForExp(exp.get()), maxLevel - 1));

		runtime::Ptr<Player> player = getPlayer();
		if (player) {
			player->getController().onLevelChange(oldLevel, level.get());
			utils::PacketSendUtility::sendPacket(*player, network::aion::serverpackets::SM_STATUPDATE_EXP(getExpShown(), getExpRecoverable(),
																getExpNeed(), getCurrentReposeEnergy(), getMaxReposeEnergy()));
		}
	}
}

bool PlayerCommonData::isHaveMentorFlag() {
	return mentorFlagTime.get() > commons::utils::currentTimeMillis() / 1000;
}

int32_t PlayerCommonData::getLastOnlineEpochSeconds() {
	std::optional<commons::database::Timestamp> timestamp = lastOnline.get();
	return !timestamp ? 0 : static_cast<int32_t>(timestamp->time_since_epoch().count() / 1000);
}

void PlayerCommonData::setLevel(int32_t levelValue) {
	setExp(experienceTable().getStartExpForLevel(levelValue));
}

runtime::Ptr<Player> PlayerCommonData::getPlayer() {
	return online.get() ? world::World::getInstance().getPlayer(playerObjId) : nullptr;
}

void PlayerCommonData::addDp(int32_t dpValue) {
	setDp(dp.get() + dpValue);
}

void PlayerCommonData::setDp(int32_t dpValue) {
	if (isStartingClass(playerClass.get()))
		return;

	int32_t maxDp = !getPlayer() ? -1 : getPlayer()->getGameStats()->getMaxDp()->getCurrent();
	dp.set((maxDp >= 0 && dpValue > maxDp) ? maxDp : dpValue);

	if (getPlayer()) {
		utils::PacketSendUtility::broadcastPacket(*getPlayer(), network::aion::serverpackets::SM_DP_INFO(playerObjId, dp.get()), true);
		getPlayer()->getGameStats()->updateStatsAndSpeedVisually();
		utils::PacketSendUtility::sendPacket(*getPlayer(), network::aion::serverpackets::SM_STATUPDATE_DP(dp.get()));
	}
}

int32_t PlayerCommonData::getTemplateId() const {
	return 100000 + getRaceId(race.get()) * 2 + getGenderId(gender.get());
}

int8_t PlayerCommonData::getCurrentSalvationPercent() {
	if (salvationPoint.get() <= 0)
		return 0;

	int64_t per = salvationPoint.get() / 1000;
	if (per > 30)
		return 30;

	return static_cast<int8_t>(per);
}

void PlayerCommonData::addSalvationPoints(int64_t points) {
	salvationPoint.set(salvationPoint.get() + points);
}

void PlayerCommonData::resetSalvationPoints() {
	salvationPoint.set(0);
}

bool PlayerCommonData::updateDaeva() {
	if (isDaeva_.get())
		return false;

	if (isStartingClass(playerClass.get()))
		return false;

	runtime::Ref<QuestStateList> qsl;
	runtime::Ptr<Player> player = getPlayer();
	if (player)
		qsl = runtime::Ref<QuestStateList>(player->getQuestStateList());
	else
		qsl = dao::PlayerQuestListDAO::load(playerObjId);

	// check both quest states in case a player changed race
	runtime::Ptr<questEngine::model::QuestState> elyAscentQuest = qsl->getQuestState(1006);
	runtime::Ptr<questEngine::model::QuestState> asmoAscentQuest = qsl->getQuestState(2008);
	std::optional<questEngine::model::QuestStatus> elyAscentQuestStatus = elyAscentQuest ? std::optional(elyAscentQuest->getStatus()) : std::nullopt;
	std::optional<questEngine::model::QuestStatus> asmoAscentQuestStatus =
		asmoAscentQuest ? std::optional(asmoAscentQuest->getStatus()) : std::nullopt;
	if (elyAscentQuestStatus != questEngine::model::QuestStatus::COMPLETE && asmoAscentQuestStatus != questEngine::model::QuestStatus::COMPLETE)
		return false;

	setDaeva(true);
	return true;
}

} // namespace aion::gameserver::model::gameobjects::player
