#include "aion/gameserver/services/HousingBidService.h"

#include <chrono>
#include <string>
#include <unordered_set>
#include <vector>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/utils/Numbers.h"
#include "aion/commons/utils/StringUtils.h"
#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/configs/main/HousingConfig.h"
#include "aion/gameserver/configs/main/LoggingConfig.h"
#include "aion/gameserver/controllers/HouseController.h"
#include "aion/gameserver/dao/HouseBidsDAO.h"
#include "aion/gameserver/dataholders/loadingutils/EnumTraits.h"
#include "aion/gameserver/model/gameobjects/Letter.h"
#include "aion/gameserver/model/gameobjects/Persistable.h"
#include "aion/gameserver/model/gameobjects/player/Mailbox.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"
#include "aion/gameserver/model/house/House.h"
#include "aion/gameserver/model/house/HouseBids.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/model/templates/housing/HouseType.h"
#include "aion/gameserver/model/templates/housing/HousingLand.h"
#include "aion/gameserver/model/templates/housing/Sale.h"
#include "aion/gameserver/network/aion/serverpackets/SM_RECEIVE_BIDS.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/services/HousingService.h"
#include "aion/gameserver/services/mail/AuctionResult.h"
#include "aion/gameserver/services/mail/AuctionResultInfo.h"
#include "aion/gameserver/services/mail/MailFormatter.h"
#include "aion/gameserver/services/player/PlayerService.h"
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

/** Java int multiplication (wraps on overflow) */
int32_t javaIntMul(int32_t a, int32_t b) {
	return static_cast<int32_t>(static_cast<uint32_t>(a) * static_cast<uint32_t>(b));
}

/** Java dereferences the house HousingService finds (a NullPointerException for an unknown object id) */
House& houseOf(const Ptr<House>& house) {
	if (!house)
		throw runtime::NullPointerException("HousingService found no house");
	return *house;
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

// Java HousingBidService.java:71-86
bool HousingBidService::auction(model::house::House& house, int64_t initialPrice) {
	runtime::Ref<HouseBids> houseBids = HouseBids::create(house.getObjectId(), initialPrice);
	Ptr<HouseBids::Bid> bid = houseBids->getHighestBid();
	if (bids.putIfAbsent(house.getObjectId(), houseBids))
		return false;
	if (house.getPersistentState() == model::gameobjects::Persistable::PersistentState::NEW) // house must exist in DB before saving a bid due to foreign key
		house.save();
	if (!dao::HouseBidsDAO::addBid(*bid)) {
		bids.remove(house.getObjectId(), houseBids);
		return false;
	}
	house.setBids(houseBids, true);
	house.getController().updateSign();
	house.getController().updateAppearance();
	return true;
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

// Java HousingBidService.java:110-137
runtime::Ptr<model::house::HouseBids::Bid> HousingBidService::bid(model::gameobjects::player::Player& player, int32_t listIndex, int64_t bidOffer) {
	Ptr<HouseBids> houseBids;
	for (const Ptr<HouseBids>& b : bids.values()) { // Java: stream().filter(listIndex).findAny()
		if (b->getListIndex() == listIndex) {
			houseBids = b;
			break;
		}
	}
	if (!isAllowedToBid(player, houseBids, bidOffer))
		return nullptr; // bid too low or bidding not allowed
	Ptr<HouseBids::Bid> previousBid = houseBids->getHighestBid();
	Ptr<HouseBids::Bid> bid = houseBids->bid(player, bidOffer);
	if (!bid) { // another bidder just bid more
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_HOUSING_CANT_BID_LOWER());
		PacketSendUtility::sendPacket(player, network::aion::serverpackets::SM_RECEIVE_BIDS(0));
		return nullptr;
	}
	if (taskmanager::tasks::housing::AuctionEndTask::getInstance().tryProlongAuction(bid->getHouseObjectId()))
		dao::HouseBidsDAO::addBid(*bid); // no need to save the bid if prolongation failed (the auction just ended)
	player.getInventory().decreaseKinah(bid->getKinah());
	Ptr<House> bidHouse = HousingService::getInstance().findHouse(bid->getHouseObjectId());
	if (previousBid != houseBids->getInitialOffer() && previousBid->getPlayerObjectId() != 0) {
		runtime::Ref<model::gameobjects::player::PlayerCommonData> prevPcd = player::PlayerService::getOrLoadPlayerCommonData(previousBid->getPlayerObjectId());
		if (!prevPcd) // Java: NullPointerException on prevPcd.isOnline()
			throw runtime::NullPointerException("No player common data for the outbid player " + std::to_string(previousBid->getPlayerObjectId()));
		if (prevPcd->isOnline()) {
			PacketSendUtility::sendPacket(*prevPcd->getPlayer(), SM_SYSTEM_MESSAGE::STR_MSG_HOUSING_BID_CANCEL());
			PacketSendUtility::sendPacket(*prevPcd->getPlayer(), network::aion::serverpackets::SM_RECEIVE_BIDS(0));
		}
		mail::MailFormatter::sendHouseAuctionMail(bidHouse, *prevPcd, mail::AuctionResult::FAILED_BID, bid->getTime(), previousBid->getKinah());
	}
	PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_HOUSING_BID_SUCCESS(houseOf(bidHouse).getAddress()->getId()));
	PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_HOUSING_PRICE_CHANGE(bidOffer));
	PacketSendUtility::sendPacket(player, network::aion::serverpackets::SM_RECEIVE_BIDS(0));
	return bid;
}

// Java HousingBidService.java:139-189
bool HousingBidService::isAllowedToBid(model::gameobjects::player::Player& player, runtime::Ptr<model::house::HouseBids> houseBids, int64_t bidOffer) {
	if (!HousingService::getInstance().canOwnHouse(player, true))
		return false;
	if (!houseBids) {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_HOUSING_BID_FAIL());
		return false;
	}
	if (!isAuctionOpen(houseBids->getHouseObjectId())) {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_HOUSING_CANT_BID_TIMEOUT());
		return false;
	}
	House& bidHouse = houseOf(HousingService::getInstance().findHouse(houseBids->getHouseObjectId()));
	if (player.getObjectId() == bidHouse.getOwnerId()) { // client usually already checks this, but we want to make sure
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_HOUSING_CANT_BID_MY_HOUSE());
		return false;
	}

	if (HousingService::getInstance().findInactiveHouse(player.getObjectId())) {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_HOUSING_CANT_BID_GRACE_HOUSE());
		return false;
	}
	Ptr<House> playerHouse = player.getActiveHouse();
	if (playerHouse && !playerHouse->isFeePaid() && configs::main::HousingConfig::ENABLE_HOUSE_PAY.load()) {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_HOUSING_CANT_BID_OVERDUE());
		return false;
	}
	int32_t minBidLevel = getMinBidLevel(bidHouse);
	if (player.getLevel() < minBidLevel) {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_HOUSING_CANT_BID_LOW_LEVEL(minBidLevel));
		return false;
	}
	Ptr<HouseBids::Bid> highestBid = houseBids->getHighestBid();
	if (highestBid->getPlayerObjectId() == player.getObjectId()) {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_HOUSING_CANT_BID_SUCC_BID_HOUSE());
		return false;
	}
	for (const Ptr<HouseBids>& b : bids.values()) { // Java: bids.values().stream().anyMatch(b -> b.isHighestBidder(player))
		if (b->isHighestBidder(player)) {
			PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_HOUSING_CANT_BID_OTHER_HOUSE());
			return false;
		}
	}
	if (player.getInventory().getKinah() < bidOffer) {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_HOUSING_CANT_BID_NOT_ENOUGH_MONEY(bidOffer));
		return false;
	}
	int64_t currentBid = highestBid->getKinah();
	// Java: bidOffer - currentBid >= currentBid * HousingConfig.AUCTION_BID_STEP_LIMIT / 100f - long * float in float, then long >= float in float
	if (static_cast<float>(bidOffer - currentBid) >= static_cast<float>(currentBid) * configs::main::HousingConfig::AUCTION_BID_STEP_LIMIT.load() / 100.0f) {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_HOUSING_CANT_BID_EXCESS_ACCOUNT());
		return false;
	}
	return true;
}

// Java HousingBidService.java:191-197
void HousingBidService::endAuctions() {
	for (int32_t houseObjectId : bids.keySet()) { // Java: bids.keySet().forEach (a weakly consistent view; endAuction removes the key)
		if (!taskmanager::tasks::housing::AuctionEndTask::getInstance().isAuctionProlonged(houseObjectId))
			endAuction(houseObjectId);
	}
	impoundAndAuctionOldPlayerHouses();
}

// Java HousingBidService.java:199-286
bool HousingBidService::endAuction(int32_t houseObjectId) {
	using mail::AuctionResult;
	taskmanager::tasks::housing::AuctionEndTask::getInstance().onAuctionEnd(houseObjectId);
	Ptr<HouseBids> houseBids;
	if (!dao::HouseBidsDAO::deleteHouseBids(houseObjectId) || !(houseBids = bids.remove(houseObjectId)))
		return false;
	Ptr<House> housePtr = HousingService::getInstance().findHouse(houseObjectId);
	House& house = houseOf(housePtr);
	house.setBids(nullptr, false);
	int32_t sellerId = house.getOwnerId();
	Ptr<model::gameobjects::player::PlayerCommonData> sellerPcd;
	if (sellerId != 0)
		sellerPcd = player::PlayerService::getOrLoadPlayerCommonData(sellerId);
	Ptr<HouseBids::Bid> highestBid = houseBids->getHighestBid();
	if (highestBid == houseBids->getInitialOffer()) {
		AuctionResult result = AuctionResult::FAILED_SALE;
		int64_t time = houseBids->getInitialOffer()->getTime(); // registration time
		int64_t compensation = 0;

		Ptr<House> inactiveHouse = sellerId == 0 ? nullptr : HousingService::getInstance().findInactiveHouse(sellerId);
		if (inactiveHouse && inactiveHouse->secondsUntilGraceEnd() == 0) {
			HousingService::getInstance().changeOwner(house, 0); // inactive house will also be activated automatically by this
			result = AuctionResult::GRACE_FAIL;
			time = commons::utils::currentTimeMillis();
			// Java: (long) (long * float)
			compensation = static_cast<int64_t>(static_cast<float>(houseBids->getInitialOffer()->getKinah()) *
				configs::main::HousingConfig::AUCTION_GRACE_END_REFUND_PERCENT.load());
		} else {
			house.getController().updateSign();
		}
		if (sellerPcd) {
			if (sellerPcd->isOnline())
				PacketSendUtility::sendPacket(*sellerPcd->getPlayer(), SM_SYSTEM_MESSAGE::STR_MSG_HOUSING_AUCTION_FAIL(house.getAddress()->getId()));
			mail::MailFormatter::sendHouseAuctionMail(housePtr, *sellerPcd, result, time, compensation);
		}
		if (configs::main::LoggingConfig::LOG_HOUSE_AUCTION.load()) {
			log.info("Address " + std::to_string(house.getAddress()->getId()) + " not sold for " +
				std::to_string(houseBids->getInitialOffer()->getKinah()) + " kinah (result: " + std::string(xml::enumName(result)) +
				"; return: " + std::to_string(compensation) + " kinah)");
		}
	} else {
		Ptr<model::gameobjects::player::PlayerCommonData> buyerPcd = player::PlayerService::getOrLoadPlayerCommonData(highestBid->getPlayerObjectId());

		if (!buyerPcd) {
			if (highestBid->getPlayerObjectId() == 0)
				log.info(house.toString() + " wasn't sold because the winning bidder deleted his character.");
			else
				log.warn(house.toString() + " could not be sold to player " + std::to_string(highestBid->getPlayerObjectId()) +
					" because the player couldn't be found");
			house.getController().updateSign();
			return true;
		}
		if (buyerPcd->getPlayerObjId() == sellerId) {
			log.warn("Sold " + house.toString() + " to its own owner (" + std::to_string(sellerId) + "), cancelling!");
			house.getController().updateSign();
			return true;
		}

		Ptr<House> studio = HousingService::getInstance().getPlayerStudio(buyerPcd->getPlayerObjId());
		if (studio)
			HousingService::getInstance().changeOwner(*studio, 0);
		HousingService::getInstance().changeOwner(house, buyerPcd->getPlayerObjId());

		AuctionResult result = AuctionResult::WIN_BID;
		int64_t time = commons::utils::currentTimeMillis();
		if (buyerPcd->isOnline())
			PacketSendUtility::sendPacket(*buyerPcd->getPlayer(), SM_SYSTEM_MESSAGE::STR_MSG_HOUSING_BID_WIN(house.getAddress()->getId()));
		if (house.isInactive()) { // buyer has another house
			if (buyerPcd->isOnline())
				PacketSendUtility::sendPacket(*buyerPcd->getPlayer(), SM_SYSTEM_MESSAGE::STR_MSG_HOUSING_GRACE_START(house.getAddress()->getId()));
			result = AuctionResult::GRACE_START;
			// Java: System.currentTimeMillis() + house.secondsUntilGraceEnd() * 1000 (int * int, then long +)
			mail::MailFormatter::sendHouseAuctionMail(housePtr, *buyerPcd, result,
				commons::utils::currentTimeMillis() + static_cast<int64_t>(javaIntMul(house.secondsUntilGraceEnd(), 1000)), 0);
		} else {
			mail::MailFormatter::sendHouseAuctionMail(housePtr, *buyerPcd, result, time, 0);
		}

		if (sellerPcd) {
			if (sellerPcd->isOnline())
				PacketSendUtility::sendPacket(*sellerPcd->getPlayer(), SM_SYSTEM_MESSAGE::STR_MSG_HOUSING_AUCTION_SUCCESS(house.getAddress()->getId()));
			Ptr<House> newHouse = HousingService::getInstance().findActiveHouse(sellerPcd->getPlayerObjId());
			if (newHouse) { // seller got his new house activated because the old one is sold
				if (sellerPcd->isOnline())
					PacketSendUtility::sendPacket(*sellerPcd->getPlayer(), SM_SYSTEM_MESSAGE::STR_MSG_HOUSING_GRACE_SUCCESS(house.getAddress()->getId()));
				mail::MailFormatter::sendHouseAuctionMail(newHouse, *sellerPcd, AuctionResult::GRACE_SUCCESS, time, highestBid->calculateSaleRewardKinah());
			} else
				mail::MailFormatter::sendHouseAuctionMail(housePtr, *sellerPcd, AuctionResult::SUCCESS_SALE, time, highestBid->calculateSaleRewardKinah());
		}

		if (configs::main::LoggingConfig::LOG_HOUSE_AUCTION.load()) {
			std::string sellerInfo = !sellerPcd ? "" : " by player " + std::to_string(sellerPcd->getPlayerObjId());
			log.info("Address " + std::to_string(house.getAddress()->getId()) + " sold" + sellerInfo + " for " + std::to_string(highestBid->getKinah()) +
				" kinah (" + std::to_string(houseBids->getBidCount()) + " bids; result: " + std::string(xml::enumName(result)) + ") to player " +
				std::to_string(buyerPcd->getPlayerObjId()));
		}
	}
	return true;
}

// Java HousingBidService.java:288-302
void HousingBidService::impoundAndAuctionOldPlayerHouses() {
	for (const Ptr<House>& house : HousingService::getInstance().getCustomHouses()) {
		if (house->isInactive() && house->secondsUntilGraceEnd() == 0) {
			Ptr<House> oldHousePtr = HousingService::getInstance().findActiveHouse(house->getOwnerId());
			House& oldHouse = houseOf(oldHousePtr); // Java: NullPointerException in changeOwner(null, 0)
			HousingService::getInstance().changeOwner(oldHouse, 0);
			runtime::Ref<model::gameobjects::player::PlayerCommonData> pcd = player::PlayerService::getOrLoadPlayerCommonData(house->getOwnerId());
			if (!pcd) // Java: NullPointerException on pcd.isOnline()
				throw runtime::NullPointerException("No player common data for house owner " + std::to_string(house->getOwnerId()));
			if (pcd->isOnline())
				PacketSendUtility::sendPacket(*pcd->getPlayer(),
					SM_SYSTEM_MESSAGE::STR_MSG_HOUSING_GRACE_FAIL(house->getAddress()->getId(), oldHouse.getAddress()->getId()));
			if (auction(oldHouse, oldHouse.getDefaultAuctionPrice()))
				mail::MailFormatter::sendHouseAuctionMail(house, *pcd, mail::AuctionResult::GRACE_FAIL, commons::utils::currentTimeMillis(),
					static_cast<int64_t>(static_cast<float>(oldHouse.getDefaultAuctionPrice()) * configs::main::HousingConfig::AUCTION_GRACE_END_REFUND_PERCENT.load()));
		}
	}
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

// Java HousingBidService.java:345-374
bool HousingBidService::cancelAuction(model::house::House& house) {
	Ptr<HouseBids> houseBids = bids.remove(house.getObjectId());
	if (!houseBids)
		return false;

	dao::HouseBidsDAO::deleteHouseBids(house.getObjectId());
	house.setBids(nullptr, true);
	house.getController().updateSign();
	house.getController().updateAppearance();

	if (house.getOwnerId() != 0) {
		runtime::Ref<model::gameobjects::player::PlayerCommonData> pcd = player::PlayerService::getOrLoadPlayerCommonData(house.getOwnerId());
		if (!pcd) // Java: NullPointerException on pcd.isOnline()
			throw runtime::NullPointerException("No player common data for house owner " + std::to_string(house.getOwnerId()));
		if (pcd->isOnline()) {
			PacketSendUtility::sendPacket(*pcd->getPlayer(), SM_SYSTEM_MESSAGE::STR_MSG_HOUSING_AUCTION_FAIL(house.getAddress()->getId()));
			PacketSendUtility::sendPacket(*pcd->getPlayer(), network::aion::serverpackets::SM_RECEIVE_BIDS(1));
		}
		mail::MailFormatter::sendHouseAuctionMail(Ptr<House>(house), *pcd, mail::AuctionResult::CANCELED_BID, commons::utils::currentTimeMillis(), 0);
	}
	Ptr<HouseBids::Bid> highestBid = houseBids->getHighestBid();
	if (highestBid != houseBids->getInitialOffer() && highestBid->getPlayerObjectId() != 0) {
		// return bid price only to the last bidder (previous bidders already get their money back when another player bids more)
		runtime::Ref<model::gameobjects::player::PlayerCommonData> pcd = player::PlayerService::getOrLoadPlayerCommonData(highestBid->getPlayerObjectId());
		if (!pcd) // Java: NullPointerException in MailFormatter (playerData.getPlayerObjId())
			throw runtime::NullPointerException("No player common data for bidder " + std::to_string(highestBid->getPlayerObjectId()));
		mail::MailFormatter::sendHouseAuctionMail(Ptr<House>(house), *pcd, mail::AuctionResult::CANCELED_BID, commons::utils::currentTimeMillis(),
			highestBid->getKinah());
	}

	return true;
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
