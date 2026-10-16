#include "aion/gameserver/network/aion/serverpackets/detail/PacketLookups.h"

#include <memory>
#include <type_traits>
#include <unordered_set>
#include <utility>

#include "aion/gameserver/dao/MailDAO.h"
#include "aion/gameserver/dao/PlayerSettingsDAO.h"
#include "aion/gameserver/model/drop/DropItem.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"
#include "aion/gameserver/model/gameobjects/player/PlayerSettings.h"
#include "aion/gameserver/model/house/House.h"
#include "aion/gameserver/model/legionDominion/LegionDominionLocation.h"
#include "aion/gameserver/model/limiteditems/LimitedTradeNpc.h"
#include "aion/gameserver/model/siege/SiegeLocation.h"
#include "aion/gameserver/model/stats/calc/Stat2.h"
#include "aion/gameserver/model/stats/container/CreatureGameStats.h"
#include "aion/gameserver/model/stats/container/CreatureLifeStats.h"
#include "aion/gameserver/model/team/legion/Legion.h"
#include "aion/gameserver/model/team/legion/LegionMember.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/services/AtreianPassportService.h"
#include "aion/gameserver/services/BrokerService.h"
#include "aion/gameserver/services/HousingService.h"
#include "aion/gameserver/services/LegionDominionService.h"
#include "aion/gameserver/services/LegionService.h"
#include "aion/gameserver/services/LimitedItemTradeService.h"
#include "aion/gameserver/services/RepurchaseService.h"
#include "aion/gameserver/services/SiegeService.h"
#include "aion/gameserver/services/TownService.h"
#include "aion/gameserver/services/abyss/AbyssRankingCache.h"
#include "aion/gameserver/services/conquerorAndProtectorSystem/CPInfo.h"
#include "aion/gameserver/services/conquerorAndProtectorSystem/ConquerorAndProtectorService.h"
#include "aion/gameserver/services/drop/DropRegistrationService.h"
#include "aion/gameserver/services/player/MultiClientingService.h"
#include "aion/gameserver/services/trade/PricesService.h"

namespace aion::gameserver::network::aion::serverpackets::detail {

namespace {

// setPacketLookupsForTests (C++ only test seam); nullptr: the Java expressions
runtime::Field<const PacketLookupsForTests*> testLookups{nullptr};

/** The installed test lookup for `member`, or nullptr */
template <class Member>
auto lookupFor(Member PacketLookupsForTests::* member) noexcept -> std::remove_cvref_t<decltype(std::declval<PacketLookupsForTests>().*member)> {
	const PacketLookupsForTests* lookups = testLookups.get();
	return lookups == nullptr ? nullptr : lookups->*member;
}

} // namespace

runtime::Ptr<model::team::legion::LegionMember> getLegionMember(model::gameobjects::player::PlayerCommonData& playerCommonData) {
	if (auto lookup = lookupFor(&PacketLookupsForTests::legionMemberOfCommonData))
		return lookup(playerCommonData);
	return services::LegionService::getInstance().getLegionMember(playerCommonData);
}

runtime::Ptr<model::team::legion::LegionMember> getLegionMember(int32_t playerObjId) {
	if (auto lookup = lookupFor(&PacketLookupsForTests::legionMemberOfPlayerId))
		return lookup(playerObjId);
	return services::LegionService::getInstance().getLegionMember(playerObjId);
}

runtime::Ptr<model::team::legion::Legion> getLegion(int32_t legionId) {
	if (auto lookup = lookupFor(&PacketLookupsForTests::legion))
		return lookup(legionId);
	return services::LegionService::getInstance().getLegion(legionId);
}

int32_t loadDisplaySettings(int32_t playerId) {
	if (auto lookup = lookupFor(&PacketLookupsForTests::displaySettings))
		return lookup(playerId);
	return dao::PlayerSettingsDAO::loadSettings(playerId)->getDisplay();
}

bool haveUnreadMail(int32_t playerId) {
	if (auto lookup = lookupFor(&PacketLookupsForTests::unreadMail))
		return lookup(playerId);
	return dao::MailDAO::haveUnread(playerId);
}

int64_t getEarnedKinahFromSoldItems(model::gameobjects::player::PlayerCommonData& playerCommonData) {
	if (auto lookup = lookupFor(&PacketLookupsForTests::earnedKinahFromSoldItems))
		return lookup(playerCommonData);
	return services::BrokerService::getInstance().getEarnedKinahFromSoldItems(playerCommonData);
}

std::optional<int32_t> checkForFactionSwitchCooldownTime(model::Race race, AionConnection* con) {
	if (auto lookup = lookupFor(&PacketLookupsForTests::factionSwitchCooldownTime))
		return lookup(race, con);
	return services::player::MultiClientingService::checkForFactionSwitchCooldownTime(race, con);
}

int32_t getTownIdByPosition(model::gameobjects::Creature& creature) {
	if (auto lookup = lookupFor(&PacketLookupsForTests::townIdByPosition))
		return lookup(creature);
	return services::TownService::getInstance().getTownIdByPosition(creature);
}

runtime::Ptr<model::house::House> findActiveHouse(int32_t playerObjId) {
	if (auto lookup = lookupFor(&PacketLookupsForTests::activeHouse))
		return lookup(playerObjId);
	return services::HousingService::getInstance().findActiveHouse(playerObjId);
}

runtime::Ptr<model::house::House> getActiveHouse(model::gameobjects::player::Player& player) {
	if (auto lookup = lookupFor(&PacketLookupsForTests::activeHouseOfPlayer))
		return lookup(player);
	return player.getActiveHouse();
}

int32_t getRankingListPosition(model::team::legion::Legion& legion) {
	if (auto lookup = lookupFor(&PacketLookupsForTests::rankingListPosition))
		return lookup(legion);
	return services::abyss::AbyssRankingCache::getInstance().getRankingListPosition(legion);
}

std::vector<runtime::Ptr<model::legionDominion::LegionDominionLocation>> getLegionDominions() {
	if (auto lookup = lookupFor(&PacketLookupsForTests::legionDominions))
		return lookup();
	return services::LegionDominionService::getInstance().getLegionDominions();
}

runtime::Ptr<services::conquerorAndProtectorSystem::CPInfo> getCPInfoForCurrentMap(model::gameobjects::player::Player& player) {
	if (auto lookup = lookupFor(&PacketLookupsForTests::cpInfoForCurrentMap))
		return lookup(player);
	return services::conquerorAndProtectorSystem::ConquerorAndProtectorService::getInstance().getCPInfoForCurrentMap(player);
}

runtime::Ptr<model::siege::SiegeLocation> getSiegeLocation(int32_t id) {
	if (auto lookup = lookupFor(&PacketLookupsForTests::siegeLocation))
		return lookup(id);
	return services::SiegeService::getInstance().getSiegeLocation(id);
}

std::vector<std::pair<int32_t, runtime::Ptr<model::siege::SiegeLocation>>> getSiegeLocations() {
	if (auto lookup = lookupFor(&PacketLookupsForTests::siegeLocations))
		return lookup();
	std::vector<std::pair<int32_t, runtime::Ptr<model::siege::SiegeLocation>>> locations;
	for (const auto& entry : services::SiegeService::getInstance().getSiegeLocations().snapshot())
		locations.emplace_back(entry.key, entry.value);
	return locations;
}

int32_t getRemainingSiegeTimeInSeconds(int32_t siegeLocationId) {
	if (auto lookup = lookupFor(&PacketLookupsForTests::remainingSiegeTimeInSeconds))
		return lookup(siegeLocationId);
	return services::SiegeService::getInstance().getRemainingSiegeTimeInSeconds(siegeLocationId);
}

int32_t getGlobalPrices(model::Race playerRace) {
	if (auto lookup = lookupFor(&PacketLookupsForTests::globalPrices))
		return lookup(playerRace);
	return services::trade::PricesService::getGlobalPrices(playerRace);
}

int32_t getGlobalPricesModifier() {
	if (auto lookup = lookupFor(&PacketLookupsForTests::globalPricesModifier))
		return lookup();
	return services::trade::PricesService::getGlobalPricesModifier();
}

int32_t getTaxes(model::Race playerRace) {
	if (auto lookup = lookupFor(&PacketLookupsForTests::taxes))
		return lookup(playerRace);
	return services::trade::PricesService::getTaxes(playerRace);
}

int32_t getVendorSellModifier() {
	if (auto lookup = lookupFor(&PacketLookupsForTests::vendorSellModifier))
		return lookup();
	return services::trade::PricesService::getVendorSellModifier();
}

std::vector<runtime::Ptr<model::drop::DropItem>> getCurrentDropItems(int32_t targetObjectId) {
	if (auto lookup = lookupFor(&PacketLookupsForTests::currentDropItems))
		return lookup(targetObjectId);
	std::vector<runtime::Ptr<model::drop::DropItem>> items;
	if (auto set = services::drop::DropRegistrationService::getInstance().getCurrentDropMap().get(targetObjectId)) {
		for (const auto& item : set->snapshot())
			items.emplace_back(item);
	}
	return items;
}

std::vector<runtime::Ptr<model::gameobjects::Item>> getRepurchaseItems(int32_t playerObjectId) {
	if (auto lookup = lookupFor(&PacketLookupsForTests::repurchaseItems))
		return lookup(playerObjectId);
	std::unordered_set<runtime::Ptr<model::gameobjects::Item>> items = services::RepurchaseService::getInstance().getRepurchaseItems(playerObjectId);
	return {items.begin(), items.end()};
}

runtime::Ptr<model::limiteditems::LimitedTradeNpc> getLimitedTradeNpc(int32_t npcId) {
	if (auto lookup = lookupFor(&PacketLookupsForTests::limitedTradeNpc))
		return lookup(npcId);
	return services::LimitedItemTradeService::getInstance().getLimitedTradeNpc(npcId);
}

bool isAtreianPassportDisabled() {
	if (auto lookup = lookupFor(&PacketLookupsForTests::atreianPassportDisabled))
		return lookup();
	return services::AtreianPassportService::getInstance().isAtreianPassportDisabled();
}

int32_t getHpPercentage(model::gameobjects::Creature& creature) {
	if (auto lookup = lookupFor(&PacketLookupsForTests::hpPercentage))
		return lookup(creature);
	return creature.getLifeStats()->getHpPercentage();
}

int32_t getMaxHpCurrent(model::gameobjects::Creature& creature) {
	if (auto lookup = lookupFor(&PacketLookupsForTests::maxHpCurrent))
		return lookup(creature);
	return creature.getGameStats()->getMaxHp()->getCurrent();
}

float getMovementSpeedFloat(model::gameobjects::Creature& creature) {
	if (auto lookup = lookupFor(&PacketLookupsForTests::movementSpeedFloat))
		return lookup(creature);
	return creature.getGameStats()->getMovementSpeedFloat();
}

AttackSpeedValues getAttackSpeed(model::gameobjects::Creature& creature) {
	if (auto lookup = lookupFor(&PacketLookupsForTests::attackSpeed))
		return lookup(creature);
	std::unique_ptr<model::stats::calc::Stat2> attackSpeed = creature.getGameStats()->getAttackSpeed();
	return AttackSpeedValues{attackSpeed->getBase(), attackSpeed->getCurrent()};
}

void setPacketLookupsForTests(const PacketLookupsForTests* lookups) noexcept {
	testLookups.set(lookups);
}

} // namespace aion::gameserver::network::aion::serverpackets::detail
