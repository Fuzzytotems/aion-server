#include "aion/gameserver/services/LegionDominionService.h"

#include <chrono>
#include <string>
#include <unordered_map>
#include <vector>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/dao/LegionDAO.h"
#include "aion/gameserver/model/gameobjects/LetterType.h"
#include "aion/gameserver/model/team/legion/Legion.h"
#include "aion/gameserver/model/team/legion/LegionMember.h"
#include "aion/gameserver/network/aion/serverpackets/SM_LEGION_DOMINION_LOC_INFO.h"
#include "aion/gameserver/network/aion/serverpackets/SM_LEGION_DOMINION_RANK.h"
#include "aion/gameserver/network/aion/serverpackets/SM_LEGION_INFO.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/services/LegionService.h"
#include "aion/gameserver/services/mail/SystemMailService.h"
#include "aion/gameserver/utils/PacketSendUtility.h"

#include "aion/gameserver/dao/LegionDominionDAO.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/LegionDominionData.h"
#include "aion/gameserver/model/legionDominion/LegionDominionLocation.h"
#include "aion/gameserver/model/legionDominion/LegionDominionParticipantInfo.h"
#include "aion/gameserver/model/templates/LegionDominionLocationTemplate.h"
#include "aion/gameserver/model/Race.h"
#include "aion/gameserver/model/templates/LegionDominionInvasionRift.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/services/RiftService.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"
#include "aion/gameserver/utils/time/ServerTime.h"

namespace aion::gameserver::services {

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.services.LegionDominionService");

LegionDominionService::LegionDominionService() = default;

LegionDominionService::~LegionDominionService() = default;

LegionDominionService& LegionDominionService::getInstance() {
	static LegionDominionService instance; // Java SingletonHolder
	return instance;
}

void LegionDominionService::initLocations() {
	using model::legionDominion::LegionDominionLocation;
	using model::legionDominion::LegionDominionParticipantInfo;
	for (const model::templates::LegionDominionLocationTemplate& temp : dataholders::DataManager::LEGION_DOMINION_DATA->getLocationTemplates()) {
		legionDominionLocations.put(temp.getId(), LegionDominionLocation::create(&temp));
	}
	// Java passes the TreeMap itself; the DAO takes a borrowed view of it
	std::unordered_map<int32_t, runtime::Ptr<LegionDominionLocation>> locations;
	for (const auto& entry : legionDominionLocations.snapshot())
		locations.emplace(entry.key, entry.value);
	dao::LegionDominionDAO::loadOrCreateLegionDominionLocations(locations);
	for (const runtime::Ptr<LegionDominionLocation>& loc : legionDominionLocations.values()) {
		// Java: loc.setParticipantInfo(LegionDominionDAO.loadParticipants(loc)) stores the loaded TreeMap
		runtime::Ref<runtime::RcTreeMap<int32_t, runtime::Ref<LegionDominionParticipantInfo>>> participants =
			runtime::RcTreeMap<int32_t, runtime::Ref<LegionDominionParticipantInfo>>::create(AION_LOCK_CLASS(LegionDominionLocation::participantInfo));
		participants->putAll(dao::LegionDominionDAO::loadParticipants(*loc));
		loc->setParticipantInfo(participants);
	}
}

std::vector<runtime::Ptr<model::legionDominion::LegionDominionLocation>> LegionDominionService::getLegionDominions() {
	return legionDominionLocations.values();
}

runtime::Ptr<model::legionDominion::LegionDominionLocation> LegionDominionService::getLegionDominionLoc(int32_t locId) {
	return legionDominionLocations.get(locId);
}

// Java LegionDominionService.java:55-57
bool LegionDominionService::join(int32_t legionId, int32_t locId) {
	runtime::Ptr<model::legionDominion::LegionDominionLocation> loc = legionDominionLocations.get(locId);
	if (!loc)
		throw runtime::NullPointerException("LegionDominionLocation " + std::to_string(locId)); // Java: get(locId).join on null
	return loc->join(legionId);
}

// Java LegionDominionService.java:59-73
void LegionDominionService::onFinishInstance(runtime::Ptr<model::team::legion::Legion> legion, int32_t points, int64_t time) {
	if (legion) {
		runtime::Ptr<model::legionDominion::LegionDominionLocation> loc = getLegionDominionLoc(legion->getCurrentLegionDominion());
		if (loc) {
			runtime::Ptr<model::legionDominion::LegionDominionParticipantInfo> info = loc->getParticipantInfo(legion->getLegionId());
			if (info && info->getPoints() < points) {
				info->setDate(commons::database::Timestamp(std::chrono::milliseconds(commons::utils::currentTimeMillis())));
				info->setPoints(points);
				info->setTime(static_cast<int32_t>(time / 1000));
				dao::LegionDominionDAO::updateInfo(*info);
				loc->updateRanking();
			}
		}
	}
}

void LegionDominionService::startWeeklyCalculation() {
	using model::legionDominion::LegionDominionParticipantInfo;
	for (const runtime::Ref<model::legionDominion::LegionDominionLocation>& loc : legionDominionLocations.values()) {
		// determine winner
		std::vector<runtime::Ptr<LegionDominionParticipantInfo>> legionRanking = loc->getLegionRanking(true);
		int32_t newOccupyingLegionId = 0;

		// reset current occupying legion
		int32_t previousOccupyingLegionId = loc->getLegionId();
		if (previousOccupyingLegionId != 0) {
			runtime::Ptr<model::team::legion::Legion> legion = LegionService::getInstance().getLegion(previousOccupyingLegionId);
			updateLegionOccupation(legion, *loc, false);
		}

		// find winner of stonespear reach challenge
		runtime::Ptr<LegionDominionParticipantInfo> winner = legionRanking.empty() ? nullptr : legionRanking[0];
		if (winner) {
			runtime::Ptr<model::team::legion::Legion> winningLegion = LegionService::getInstance().getLegion(winner->getLegionId());
			if (winningLegion) {
				if (!winningLegion->isDisbanding()) {
					newOccupyingLegionId = winningLegion->getLegionId();
				} else {
					log.warn("[Legion dominion] Skipped occupy of location {} for legion [id={}, name={}] due to disbanding", loc->getLocationId(),
						winningLegion->getLegionId(), winningLegion->getName());
				}
			}
		}

		loc->setLegionId(newOccupyingLegionId);
		loc->setOccupiedDate(commons::database::Timestamp(std::chrono::milliseconds(commons::utils::currentTimeMillis())));

		// update all participated legions & store them to db
		for (const runtime::Ref<LegionDominionParticipantInfo>& info : loc->getParticipantInfo()->values()) {
			// skip previous legion since its already updated and didnt reoccupy it
			if (info->getLegionId() != previousOccupyingLegionId || info->getLegionId() == newOccupyingLegionId) {
				runtime::Ptr<model::team::legion::Legion> legion = LegionService::getInstance().getLegion(info->getLegionId());
				if (legion) {
					int32_t occupiedId = 0;
					if (winner && legion->getLegionId() == newOccupyingLegionId)
						occupiedId = loc->getLocationId();
					updateLegionOccupation(legion, *loc, occupiedId > 0);
				}
			}
			dao::LegionDominionDAO::delete_(*info);
		}

		std::unordered_map<int32_t, std::vector<const model::templates::LegionDominionReward*>> dominionRewards = loc->getRewards();
		for (size_t i = 0; i < legionRanking.size(); i++) {
			if (i >= dominionRewards.size())
				break;
			runtime::Ptr<LegionDominionParticipantInfo> participantInfo = legionRanking[i];
			runtime::Ptr<model::team::legion::Legion> legion = LegionService::getInstance().getLegion(participantInfo->getLegionId());
			if (!legion || legion->isDisbanding())
				continue;
			// Java: dominionRewards.get(i + 1).isEmpty() - a rank without rewards is null there (NullPointerException)
			auto legionRewards = dominionRewards.find(static_cast<int32_t>(i) + 1);
			if (legionRewards == dominionRewards.end())
				throw runtime::NullPointerException("no legion dominion rewards for rank " + std::to_string(i + 1));
			if (!legionRewards->second.empty()) {
				std::string playerName = legion->getBrigadeGeneral()->getName();
				for (const model::templates::LegionDominionReward* reward : legionRewards->second) {
					// TODO send proper system (most likely $$GD_REWARD_MAIL) mail
					mail::SystemMailService::sendMail("Legion Dominion", playerName, "Reward Mail", "", reward->getItemId(), reward->getCount(), 0,
						model::gameobjects::LetterType::NORMAL);
				}
			}
		}
		// reset locations participant info and update this location
		loc->reset();
		dao::LegionDominionDAO::updateLegionDominionLocation(*loc);
	}
	utils::PacketSendUtility::broadcastToWorld(network::aion::serverpackets::SM_LEGION_DOMINION_LOC_INFO());
}

void LegionDominionService::updateLegionOccupation(runtime::Ptr<model::team::legion::Legion> legion, model::legionDominion::LegionDominionLocation& location, bool shouldOccupy) {
	if (!legion)
		return;
	legion->setOccupiedLegionDominion(shouldOccupy ? location.getLocationId() : 0);
	legion->setLastLegionDominion(location.getLocationId());
	legion->setCurrentLegionDominion(0);
	dao::LegionDAO::storeLegion(*legion);
	utils::PacketSendUtility::broadcastToLegion(*legion, network::aion::serverpackets::SM_LEGION_DOMINION_RANK(location, legion));
	utils::PacketSendUtility::broadcastToLegion(*legion, network::aion::serverpackets::SM_LEGION_INFO(*legion));
}

// Java LegionDominionService.java:167-170
bool LegionDominionService::isInCalculationTime() {
	const auto now = utils::time::ServerTime::now().get_local_time();
	const std::chrono::local_days day = std::chrono::floor<std::chrono::days>(now);
	const int64_t hour = std::chrono::hh_mm_ss(now - day).hours().count();
	return std::chrono::weekday(day) == std::chrono::Wednesday && hour >= 8 && hour <= 10;
}

// callback at LegionDominionService.java:188 (fieldmap key LegionDominionService@L188:45)
// Java LegionDominionService.java:172-194
bool LegionDominionService::openInvasionRift(int32_t territoryId) {
	runtime::Ptr<model::legionDominion::LegionDominionLocation> location = getLegionDominionLoc(territoryId);
	if (!location || location->getInvasionRift() == nullptr)
		return false;
	const model::templates::LegionDominionInvasionRift* invasionRift = location->getInvasionRift();
	if (isInCalculationTime())
		return false;
	if (RiftService::getInstance().isRiftOpened(invasionRift->getRiftId()))
		return false;
	const bool openedSuccessfully = RiftService::getInstance().openRifts(invasionRift->getRiftId(), false);
	if (openedSuccessfully) {
		if (location->getRace() == model::Race::ELYOS) {
			utils::PacketSendUtility::broadcastToWorld(network::aion::serverpackets::SM_SYSTEM_MESSAGE::STR_MSG_LIGHT_SIDE_LEGION_DIRECT_PORTAL_OPEN());
		} else {
			utils::PacketSendUtility::broadcastToWorld(network::aion::serverpackets::SM_SYSTEM_MESSAGE::STR_MSG_DARK_SIDE_LEGION_DIRECT_PORTAL_OPEN());
		}
		// Schedule rift close (Java: int arithmetic, getDuration() * 3540 * 1000)
		const int32_t riftId = invasionRift->getRiftId();
		utils::ThreadPoolManager::getInstance().schedule(this, [riftId] { RiftService::getInstance().closeRifts(riftId); },
			static_cast<int64_t>(RiftService::getInstance().getDuration() * 3540 * 1000));
	}
	return openedSuccessfully;
}

} // namespace aion::gameserver::services
