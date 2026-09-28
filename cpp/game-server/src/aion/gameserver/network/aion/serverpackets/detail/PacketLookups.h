#pragma once

#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

#include "aion/gameserver/model/drop/fwd.h"
#include "aion/gameserver/model/fwd.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/house/fwd.h"
#include "aion/gameserver/model/legionDominion/fwd.h"
#include "aion/gameserver/model/limiteditems/fwd.h"
#include "aion/gameserver/model/siege/fwd.h"
#include "aion/gameserver/model/team/legion/fwd.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/services/conquerorAndProtectorSystem/fwd.h"

namespace aion::gameserver::network::aion::serverpackets::detail {

/**
 * C++ only, private to P4-17 (server packets L-Z and the Abstract* bases): the service calls of the packet bodies and the stat reads of the two
 * character blobs (SM_PLAYER_INFO, SM_NPC_INFO). Each function is the Java expression named in its comment.
 * <p>
 * Test seam (the pattern of model/items/detail/StaticDataLookups.h): while a test has installed a lookup with setPacketLookupsForTests, the
 * function calls it; otherwise it evaluates the Java expression. Most services and the stat calculation (P5-01) are not ported yet, so the
 * golden-byte tests of the packets that read them install lookups. Server code never installs lookups.
 */

/** Java: LegionService.getInstance().getLegionMember(playerCommonData) */
runtime::Ptr<model::team::legion::LegionMember> getLegionMember(model::gameobjects::player::PlayerCommonData& playerCommonData);

/** Java: LegionService.getInstance().getLegionMember(playerObjId) */
runtime::Ptr<model::team::legion::LegionMember> getLegionMember(int32_t playerObjId);

/** Java: LegionService.getInstance().getLegion(legionId) */
runtime::Ptr<model::team::legion::Legion> getLegion(int32_t legionId);

/** Java: PlayerSettingsDAO.loadSettings(playerId).getDisplay() */
int32_t loadDisplaySettings(int32_t playerId);

/** Java: MailDAO.haveUnread(playerId) */
bool haveUnreadMail(int32_t playerId);

/** Java: BrokerService.getInstance().getEarnedKinahFromSoldItems(playerCommonData) */
int64_t getEarnedKinahFromSoldItems(model::gameobjects::player::PlayerCommonData& playerCommonData);

/** Java: MultiClientingService.checkForFactionSwitchCooldownTime(race, con) (Java Integer: std::nullopt for null) */
std::optional<int32_t> checkForFactionSwitchCooldownTime(model::Race race, AionConnection* con);

/** Java: TownService.getInstance().getTownIdByPosition(creature) */
int32_t getTownIdByPosition(model::gameobjects::Creature& creature);

/** Java: HousingService.getInstance().findActiveHouse(playerObjId) */
runtime::Ptr<model::house::House> findActiveHouse(int32_t playerObjId);

/** Java: player.getActiveHouse() (the first loads the houses through HousingService.findPlayerHouses) */
runtime::Ptr<model::house::House> getActiveHouse(model::gameobjects::player::Player& player);

/** Java: AbyssRankingCache.getInstance().getRankingListPosition(legion) */
int32_t getRankingListPosition(model::team::legion::Legion& legion);

/** Java: LegionDominionService.getInstance().getLegionDominions() */
std::vector<runtime::Ptr<model::legionDominion::LegionDominionLocation>> getLegionDominions();

/** Java: ConquerorAndProtectorService.getInstance().getCPInfoForCurrentMap(player) */
runtime::Ptr<services::conquerorAndProtectorSystem::CPInfo> getCPInfoForCurrentMap(model::gameobjects::player::Player& player);

/** Java: SiegeService.getInstance().getSiegeLocation(id) */
runtime::Ptr<model::siege::SiegeLocation> getSiegeLocation(int32_t id);

/** Java: SiegeService.getInstance().getSiegeLocations() (the entries of the map, in the map's iteration order) */
std::vector<std::pair<int32_t, runtime::Ptr<model::siege::SiegeLocation>>> getSiegeLocations();

/** Java: SiegeService.getInstance().getRemainingSiegeTimeInSeconds(siegeLocationId) */
int32_t getRemainingSiegeTimeInSeconds(int32_t siegeLocationId);

/** Java: PricesService.getGlobalPrices(playerRace) */
int32_t getGlobalPrices(model::Race playerRace);

/** Java: PricesService.getGlobalPricesModifier() */
int32_t getGlobalPricesModifier();

/** Java: PricesService.getTaxes(playerRace) */
int32_t getTaxes(model::Race playerRace);

/** Java: PricesService.getVendorSellModifier() */
int32_t getVendorSellModifier();

/** Java: DropRegistrationService.getInstance().getCurrentDropMap().getOrDefault(targetObjectId, Collections.emptySet()) */
std::vector<runtime::Ptr<model::drop::DropItem>> getCurrentDropItems(int32_t targetObjectId);

/** Java: RepurchaseService.getInstance().getRepurchaseItems(playerObjectId) */
std::vector<runtime::Ptr<model::gameobjects::Item>> getRepurchaseItems(int32_t playerObjectId);

/** Java: LimitedItemTradeService.getInstance().getLimitedTradeNpc(npcId) */
runtime::Ptr<model::limiteditems::LimitedTradeNpc> getLimitedTradeNpc(int32_t npcId);

/** Java: AtreianPassportService.getInstance().isAtreianPassportDisabled() */
bool isAtreianPassportDisabled();

/** Java: creature.getLifeStats().getHpPercentage() */
int32_t getHpPercentage(model::gameobjects::Creature& creature);

/** Java: creature.getGameStats().getMaxHp().getCurrent() */
int32_t getMaxHpCurrent(model::gameobjects::Creature& creature);

/** Java: creature.getGameStats().getMovementSpeedFloat() */
float getMovementSpeedFloat(model::gameobjects::Creature& creature);

/** Java: Stat2 attackSpeed = creature.getGameStats().getAttackSpeed(); attackSpeed.getBase(), attackSpeed.getCurrent() */
struct AttackSpeedValues {
	int32_t base;
	int32_t current;
};
AttackSpeedValues getAttackSpeed(model::gameobjects::Creature& creature);

/** Test lookups; a null member keeps the Java expression for that function */
struct PacketLookupsForTests {
	runtime::Ptr<model::team::legion::LegionMember> (*legionMemberOfCommonData)(model::gameobjects::player::PlayerCommonData&) = nullptr;
	runtime::Ptr<model::team::legion::LegionMember> (*legionMemberOfPlayerId)(int32_t) = nullptr;
	runtime::Ptr<model::team::legion::Legion> (*legion)(int32_t) = nullptr;
	int32_t (*displaySettings)(int32_t) = nullptr;
	bool (*unreadMail)(int32_t) = nullptr;
	int64_t (*earnedKinahFromSoldItems)(model::gameobjects::player::PlayerCommonData&) = nullptr;
	std::optional<int32_t> (*factionSwitchCooldownTime)(model::Race, AionConnection*) = nullptr;
	int32_t (*townIdByPosition)(model::gameobjects::Creature&) = nullptr;
	runtime::Ptr<model::house::House> (*activeHouse)(int32_t) = nullptr;
	runtime::Ptr<model::house::House> (*activeHouseOfPlayer)(model::gameobjects::player::Player&) = nullptr;
	int32_t (*rankingListPosition)(model::team::legion::Legion&) = nullptr;
	std::vector<runtime::Ptr<model::legionDominion::LegionDominionLocation>> (*legionDominions)() = nullptr;
	runtime::Ptr<services::conquerorAndProtectorSystem::CPInfo> (*cpInfoForCurrentMap)(model::gameobjects::player::Player&) = nullptr;
	runtime::Ptr<model::siege::SiegeLocation> (*siegeLocation)(int32_t) = nullptr;
	std::vector<std::pair<int32_t, runtime::Ptr<model::siege::SiegeLocation>>> (*siegeLocations)() = nullptr;
	int32_t (*remainingSiegeTimeInSeconds)(int32_t) = nullptr;
	int32_t (*globalPrices)(model::Race) = nullptr;
	int32_t (*globalPricesModifier)() = nullptr;
	int32_t (*taxes)(model::Race) = nullptr;
	int32_t (*vendorSellModifier)() = nullptr;
	std::vector<runtime::Ptr<model::drop::DropItem>> (*currentDropItems)(int32_t) = nullptr;
	std::vector<runtime::Ptr<model::gameobjects::Item>> (*repurchaseItems)(int32_t) = nullptr;
	runtime::Ptr<model::limiteditems::LimitedTradeNpc> (*limitedTradeNpc)(int32_t) = nullptr;
	bool (*atreianPassportDisabled)() = nullptr;
	int32_t (*hpPercentage)(model::gameobjects::Creature&) = nullptr;
	int32_t (*maxHpCurrent)(model::gameobjects::Creature&) = nullptr;
	float (*movementSpeedFloat)(model::gameobjects::Creature&) = nullptr;
	AttackSpeedValues (*attackSpeed)(model::gameobjects::Creature&) = nullptr;
};

/** Tests only: installs the lookups, which must outlive their use (nullptr removes them) */
void setPacketLookupsForTests(const PacketLookupsForTests* lookups) noexcept;

} // namespace aion::gameserver::network::aion::serverpackets::detail
