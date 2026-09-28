#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/commons/database/SqlTypes.h"
#include "aion/gameserver/model/account/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/services/player/fwd.h"

namespace aion::gameserver::services::player {

/**
 * This class is designed to do all the work related with loading/storing players.<br>
 * Same with storing, {@link #storePlayer(com.aionemu.gameserver.model.gameobjects.player.Player)} stores all player data like appearance, items,
 * etc...
 *
 * @author SoulKeeper, Saelya, Cura
 */
class PlayerService {
public:
	/** @param oldName null for a new character (CM_CREATE_CHARACTER.java:86, CM_CHECK_NICKNAME.java:39) */
	static bool isNameUsedOrReserved(std::optional<std::string_view> oldName, std::string_view newName);
	static bool isNameUsedOrReserved(std::optional<std::string_view> oldName, std::string_view newName, int32_t nameReservationDurationDays);
	/** Stores newly created player */
	static bool storeNewPlayer(model::gameobjects::player::Player& player, std::string_view accountName, int32_t accountId);
	/** Stores player data into db */
	static void storePlayer(model::gameobjects::player::Player& player);
	static runtime::Ref<model::gameobjects::player::Player> getPlayer(int32_t playerObjId, runtime::Ptr<model::account::Account> account);
	/** This method is used for creating new players */
	static runtime::Ref<model::gameobjects::player::Player> newPlayer(model::account::PlayerAccountData& playerAccountData,
		model::account::Account& account);
	static runtime::Ref<model::gameobjects::player::PlayerCommonData> getOrLoadPlayerCommonData(int32_t playerObjId);
	static runtime::Ref<model::gameobjects::player::PlayerCommonData> getOrLoadPlayerCommonData(std::string_view name);
	/** Cancel Player deletion process if its possible. */
	static bool cancelPlayerDeletion(model::account::PlayerAccountData& accData);
	/**
	 * Starts player deletion process if its possible. If deletion is possible character should be deleted after 5 minutes.
	 */
	static void deletePlayer(model::account::PlayerAccountData& accData);
	/** Completely removes player from database */
	static void deletePlayerFromDB(int32_t playerId);
	static void deletePlayerFromDB(int32_t playerId, bool notifyServices);
private:
	/** Updates deletion time in database */
	static void storeDeletionTime(model::account::PlayerAccountData& accData);
public:
	static void storeCreationTime(int32_t objectId, std::optional<commons::database::Timestamp> creationDate);
	/** Add macro for player */
	static void addMacro(model::gameobjects::player::Player& player, int32_t macroOrder, std::string_view macroXML);
	/** Remove macro with specified index from specified player */
	static void removeMacro(model::gameobjects::player::Player& player, int32_t macroOrder);
	/** @return the name, null if no such player (BlockListDAO.java:67 tests it) */
	static std::optional<std::string> getPlayerName(int32_t objectId);

	/** C++ only (tests): how getPlayer and newPlayer create the Player; the default is VisibleObject::create<Player> */
	using PlayerFactory = runtime::Ref<model::gameobjects::player::Player> (*)(model::account::PlayerAccountData& playerAccountData,
		model::account::Account& account);
	/** C++ only (tests): called by getPlayer right after the account warehouse took the player as its actor (fault injection) */
	using LoadHook = void (*)(model::gameobjects::player::Player& player);
	/**
	 * C++ only (tests): called by storeNewPlayer after PlayerDAO.saveNewPlayer and before PlayerAppearanceDAO.store; false makes storeNewPlayer
	 * return false, which is the state a failing appearance store leaves behind (Java is not transactional there).
	 */
	using StoreNewPlayerHook = bool (*)(model::gameobjects::player::Player& player);

	/** C++ only (tests, m5a-plan.md S-10): replaces the Player creation of getPlayer and newPlayer; nullptr restores the default */
	static void setPlayerFactoryForTests(PlayerFactory factory) noexcept;
	/** C++ only (tests, m5a-plan.md S-10): installs a hook getPlayer runs after the account warehouse's setOwner; nullptr removes it */
	static void setLoadHookForTests(LoadHook hook) noexcept;
	/** C++ only (tests, m5a-plan.md S-03/S-10 case d): injects the database error of storeNewPlayer; nullptr removes it */
	static void setStoreNewPlayerHookForTests(StoreNewPlayerHook hook) noexcept;

	/**
	 * C++ only: the current actor of the account warehouse, null if there is none. The account warehouse is a part of the Account and shared by
	 * every Player created from it (m5a-plan.md S-05).
	 */
	static runtime::Ptr<model::gameobjects::player::Player> accountWarehouseActor(model::account::Account& account);
	/**
	 * C++ only: puts `previousActor` back as the account warehouse's actor unless it is `droppedPlayer` itself. The logout breakers (L3) null
	 * the shared actor for the player they drop; without this the character that is actually online would be left with a null actor, while Java
	 * leaves the (wrong but non-null) rejected player there.
	 */
	static void restoreAccountWarehouseActor(model::account::Account& account, runtime::Ptr<model::gameobjects::player::Player> previousActor,
		model::gameobjects::player::Player& droppedPlayer) noexcept;
};

} // namespace aion::gameserver::services::player
