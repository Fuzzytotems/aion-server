#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/lifetime/Parts.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/account/fwd.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/model/items/storage/fwd.h"
#include "aion/gameserver/questEngine/model/fwd.h"

namespace aion::gameserver::model::items::storage {

/**
 * S0c declaration header (docs/design/hub-headers.md §3.5). A part (parts.json: Player.inventory, regularWarehouse, petBags, cabinets;
 * Account.accountWarehouse) bound to its owner in the constructor. The actor is `SelfOrRef<Player>` (RR-5/RR-13): the owning player for the
 * player's storages, the entering player (or null) for the account warehouse. Java `new PlayerStorage(null, ACCOUNT_WAREHOUSE)` has no player
 * to bind: the C++-only constructor taking the Account binds the account as the part owner and leaves the actor null.
 * The overrides forward the IStorage operations to the Storage actor overloads (the using-declarations of Storage keep them visible).
 *
 * @author ATracer
 */
class PlayerStorage : public Storage {
private:
	runtime::SelfOrRef<gameobjects::player::Player> actor{*this};

public:
	/** Java: new PlayerStorage(owner, storageType), bound to the owning player */
	PlayerStorage(gameobjects::player::Player& owner, StorageType storageType);

	/** C++ only: Java new PlayerStorage(null, StorageType.ACCOUNT_WAREHOUSE) (AccountService.java:96), bound to the account, actor null */
	PlayerStorage(account::Account& account, StorageType storageType);

	~PlayerStorage() override;

	/** Java final */
	void setOwner(runtime::Ptr<gameobjects::player::Player> actor) override final;

	/**
	 * C++ only (zombie breaker, LogoutBreakers::breakZombieEdges; header request player-3): the current actor, null for an account warehouse
	 * without an entering player. The inventory and the warehouse return their owning player itself, so callers compare identities.
	 */
	runtime::Ptr<gameobjects::player::Player> getActor() const { return actor.get(); }

	void onLoadHandler(gameobjects::Item& item) override;

	using Storage::increaseKinah;

	void increaseKinah(int64_t amount) override;

	void increaseKinah(int64_t amount, services::item::ItemPacketService_ItemUpdateType updateType) override;

	using Storage::tryDecreaseKinah;

	bool tryDecreaseKinah(int64_t amount) override;

	bool tryDecreaseKinah(int64_t amount, services::item::ItemPacketService_ItemUpdateType updateType) override;

	using Storage::decreaseKinah;

	void decreaseKinah(int64_t amount) override;

	void decreaseKinah(int64_t amount, services::item::ItemPacketService_ItemUpdateType updateType) override;

	using Storage::increaseItemCount;

	int64_t increaseItemCount(gameobjects::Item& item, int64_t count) override;

	int64_t increaseItemCount(gameobjects::Item& item, int64_t count, services::item::ItemPacketService_ItemUpdateType updateType) override;

	using Storage::decreaseItemCount;

	int64_t decreaseItemCount(gameobjects::Item& item, int64_t count) override;

	int64_t decreaseItemCount(gameobjects::Item& item, int64_t count, services::item::ItemPacketService_ItemUpdateType updateType) override;

	int64_t decreaseItemCount(gameobjects::Item& item, int64_t count, services::item::ItemPacketService_ItemUpdateType updateType,
		questEngine::model::QuestStatus questStatus) override;

	using Storage::add;

	runtime::Ptr<gameobjects::Item> add(gameobjects::Item& item) override;

	runtime::Ptr<gameobjects::Item> add(gameobjects::Item& item, services::item::ItemPacketService_ItemAddType addType) override;

	using Storage::put;

	runtime::Ptr<gameobjects::Item> put(gameobjects::Item& item) override;

	using Storage::delete_;

	runtime::Ptr<gameobjects::Item> delete_(gameobjects::Item& item) override;

	runtime::Ptr<gameobjects::Item> delete_(gameobjects::Item& item, services::item::ItemPacketService_ItemDeleteType deleteType) override;

	using Storage::decreaseByItemId;

	bool decreaseByItemId(int32_t itemId, int64_t count) override;

	bool decreaseByItemId(int32_t itemId, int64_t count, questEngine::model::QuestStatus questStatus) override;

	using Storage::decreaseByObjectId;

	bool decreaseByObjectId(int32_t itemObjId, int64_t count) override;

	bool decreaseByObjectId(int32_t itemObjId, int64_t count, questEngine::model::QuestStatus questStatus) override;

	bool decreaseByObjectId(int32_t itemObjId, int64_t count, services::item::ItemPacketService_ItemUpdateType updateType) override;
};

} // namespace aion::gameserver::model::items::storage
