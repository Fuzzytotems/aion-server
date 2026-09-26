#include "aion/gameserver/services/HousingBidService.h"

#include <chrono>
#include <string>
#include <unordered_set>
#include <vector>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/utils/Numbers.h"
#include "aion/commons/utils/StringUtils.h"
#include "aion/gameserver/configs/main/HousingConfig.h"
#include "aion/gameserver/dao/HouseBidsDAO.h"
#include "aion/gameserver/model/gameobjects/Letter.h"
#include "aion/gameserver/model/gameobjects/player/Mailbox.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/house/House.h"
#include "aion/gameserver/model/house/HouseBids.h"
#include "aion/gameserver/model/templates/housing/HouseType.h"
#include "aion/gameserver/model/templates/housing/HousingLand.h"
#include "aion/gameserver/model/templates/housing/Sale.h"
#include "aion/gameserver/network/aion/serverpackets/SM_RECEIVE_BIDS.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/services/HousingService.h"
#include "aion/gameserver/services/mail/AuctionResultInfo.h"
#include "aion/gameserver/taskmanager/tasks/housing/AuctionEndTask.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/utils/time/ServerTime.h"

namespace aion::gameserver::services {

static const auto log = commons::logging::LoggerFactory::getLogger("HOUSE_AUCTION_LOG");

using model::house::House;
using model::house::HouseBids;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;
using runtime::Ptr;
using utils::PacketSendUtility;

namespace {

/** Java: String[] parts = s.split(","); parts[index] */
std::string partAt(const std::vector<std::string>& parts, size_t index) {
	if (index >= parts.size())
		throw runtime::ArrayIndexOutOfBoundsException("Index " + std::to_string(index) + " out of bounds for length " + std::to_string(parts.size()));
	return parts[index];
}

} // namespace

HousingBidService::HousingBidService() {
	std::unordered_set<int32_t> deletedPlayerIds = dao::HouseBidsDAO::loadBids(bids);
	for (int32_t deletedPlayerId : deletedPlayerIds)
		disableBids(deletedPlayerId);
	setBidInfoToHouses();
	log.info("Loaded bids for " + std::to_string(bids.size()) + " houses");
}

HousingBidService::~HousingBidService() = default;

HousingBidService& HousingBidService::getInstance() {
	static HousingBidService instance; // Java SingletonHolder
	return instance;
}

void HousingBidService::setBidInfoToHouses() {
	for (const Ptr<House>& house : HousingService::getInstance().getCustomHouses()) {
		house->setBids(getBidInfo(*house), true);
		if (house->getBids() && house->isInactive())
			log.warn(house->toString() + " is for auction but inactive.");
	}
}

bool HousingBidService::isRegisteringAllowed() {
	if (!configs::main::HousingConfig::ENABLE_HOUSE_AUCTIONS.load())
		return false;
	// Java: ServerTime.now().getDayOfWeek().getValue() - Monday 1 ... Sunday 7
	std::chrono::local_days localDay = std::chrono::floor<std::chrono::days>(utils::time::ServerTime::now().get_local_time());
	int32_t today = static_cast<int32_t>(std::chrono::weekday(localDay).iso_encoding());
	auto registerDays = configs::main::HousingConfig::HOUSE_AUCTION_REGISTER_DAYS.get();
	if (!registerDays) // Java: NullPointerException on the null array
		throw runtime::NullPointerException("HousingConfig.HOUSE_AUCTION_REGISTER_DAYS is null");
	if (registerDays->size() < 2)
		throw runtime::ArrayIndexOutOfBoundsException("Index 1 out of bounds for length " + std::to_string(registerDays->size()));
	int32_t from = (*registerDays)[0];
	int32_t to = (*registerDays)[1];
	if (from > to) // e.g. saturday (6) to wednesday (3)
		return from <= today || to >= today;
	else // e.g. monday (1) to friday (5)
		return from <= today && to >= today;
}

bool HousingBidService::auction(model::house::House& house, int64_t initialPrice) {
	AION_UNPORTED();
}

bool HousingBidService::isAuctionOpen(int32_t houseObjectId) {
	return bids.containsKey(houseObjectId) && isBiddingTime(houseObjectId);
}

bool HousingBidService::isBiddingTime(int32_t houseObjectId) {
	std::chrono::local_time<std::chrono::milliseconds> now = utils::time::ServerTime::now().get_local_time();
	std::chrono::local_days today = std::chrono::floor<std::chrono::days>(now);
	auto hour = std::chrono::duration_cast<std::chrono::hours>(now - today).count();
	return std::chrono::weekday(today) != std::chrono::Sunday || hour < 12 ||
		taskmanager::tasks::housing::AuctionEndTask::getInstance().isAuctionProlonged(houseObjectId);
}

runtime::Ptr<model::house::HouseBids> HousingBidService::getBidInfo(model::house::House& house) {
	return bids.get(house.getObjectId());
}

std::vector<runtime::Ptr<model::house::HouseBids>> HousingBidService::getBidInfo(model::Race race) {
	std::vector<Ptr<HouseBids>> houseBids;
	for (const Ptr<HouseBids>& bidInfo : bids.values()) {
		if (HousingService::getInstance().findHouse(bidInfo->getHouseObjectId())->matchesLandRace(race))
			houseBids.push_back(bidInfo);
	}
	return houseBids;
}

runtime::Ptr<model::house::HouseBids::Bid> HousingBidService::bid(model::gameobjects::player::Player& player, int32_t listIndex, int64_t bidOffer) {
	AION_UNPORTED();
}

bool HousingBidService::isAllowedToBid(model::gameobjects::player::Player& player, runtime::Ptr<model::house::HouseBids> houseBids, int64_t bidOffer) {
	AION_UNPORTED();
}

void HousingBidService::endAuctions() {
	AION_UNPORTED();
}

bool HousingBidService::endAuction(int32_t houseObjectId) {
	AION_UNPORTED();
}

void HousingBidService::impoundAndAuctionOldPlayerHouses() {
	AION_UNPORTED();
}

int32_t HousingBidService::getMinBidLevel(model::house::House& house) {
	using configs::main::HousingConfig;
	switch (house.getHouseType()) {
		case model::templates::housing::HouseType::HOUSE:
			if (HousingConfig::HOUSE_MIN_BID_LEVEL.load() > 0)
				return HousingConfig::HOUSE_MIN_BID_LEVEL.load();
			break;
		case model::templates::housing::HouseType::MANSION:
			if (HousingConfig::MANSION_MIN_BID_LEVEL.load() > 0)
				return HousingConfig::MANSION_MIN_BID_LEVEL.load();
			break;
		case model::templates::housing::HouseType::ESTATE:
			if (HousingConfig::ESTATE_MIN_BID_LEVEL.load() > 0)
				return HousingConfig::ESTATE_MIN_BID_LEVEL.load();
			break;
		case model::templates::housing::HouseType::PALACE:
			if (HousingConfig::PALACE_MIN_BID_LEVEL.load() > 0)
				return HousingConfig::PALACE_MIN_BID_LEVEL.load();
			break;
		default:
			break;
	}
	return house.getLand()->getSaleOptions()->getMinLevel();
}

void HousingBidService::disableBids(int32_t playerObjId) {
	std::vector<Ptr<HouseBids::Bid>> deletedBids;
	std::vector<runtime::Ref<HouseBids::Bid>> deletedRefs; // keeps the removed bids alive until the DAO call returns
	for (const Ptr<HouseBids>& b : bids.values()) {
		for (runtime::Ref<HouseBids::Bid>& deleted : b->deleteOrDisableBids(playerObjId)) {
			deletedBids.push_back(deleted);
			deletedRefs.push_back(std::move(deleted));
		}
	}
	dao::HouseBidsDAO::deleteOrDisableBids(playerObjId, deletedBids);
}

runtime::Ptr<model::house::HouseBids::Bid> HousingBidService::findLastBid(model::gameobjects::player::Player& player) {
	Ptr<HouseBids::Bid> result;
	for (const Ptr<HouseBids>& houseBids : bids.values()) {
		Ptr<HouseBids::Bid> latest = houseBids->getLatestBid(player);
		if (!latest)
			continue;
		// Java: reduce(BinaryOperator.maxBy(comparing(getTime))) keeps the earlier element on ties
		if (!result || latest->getTime() > result->getTime())
			result = latest;
	}
	return result;
}

runtime::Ptr<model::house::HouseBids> HousingBidService::findBidsForRegisteredHouse(model::gameobjects::player::Player& player) {
	for (const Ptr<House>& house : *player.getHouses()) {
		if (house->getBids())
			return house->getBids();
	}
	return nullptr;
}

bool HousingBidService::cancelAuction(model::house::House& house) {
	AION_UNPORTED();
}

void HousingBidService::onPlayerLogin(model::gameobjects::player::Player& player) {
	using mail::AuctionResult;
	std::vector<Ptr<model::gameobjects::Letter>> letters = player.getMailbox()->getNewSystemLetters("$$HS_AUCTION_MAIL");
	bool needsRefresh = false;

	for (const Ptr<model::gameobjects::Letter>& letter : letters) {
		std::vector<std::string> titleParts = commons::utils::StringUtils::splitJava(letter->getTitle(), ",");
		std::vector<std::string> bodyParts = commons::utils::StringUtils::splitJava(letter->getMessage(), ",");
		std::optional<AuctionResult> result = mail::auctionResultOf(commons::utils::parseInt(partAt(titleParts, 0)));
		if (result == AuctionResult::FAILED_BID) {
			needsRefresh = true;
			PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_HOUSING_BID_CANCEL());
		} else if (result == AuctionResult::WIN_BID || result == AuctionResult::GRACE_START) {
			needsRefresh = true;
			int32_t address = commons::utils::parseInt(partAt(bodyParts, 1));
			PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_HOUSING_BID_WIN(address));
		} else if (result == AuctionResult::FAILED_SALE) {
			needsRefresh = true;
			int32_t address = commons::utils::parseInt(partAt(bodyParts, 1));
			PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_HOUSING_AUCTION_FAIL(address));
		} else if (result == AuctionResult::SUCCESS_SALE || result == AuctionResult::GRACE_SUCCESS) {
			needsRefresh = true;
			int32_t address = commons::utils::parseInt(partAt(bodyParts, 1));
			PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_HOUSING_AUCTION_SUCCESS(address));
		}
	}

	if (needsRefresh)
		PacketSendUtility::sendPacket(player, network::aion::serverpackets::SM_RECEIVE_BIDS(0));
}

} // namespace aion::gameserver::services
