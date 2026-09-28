#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Parts.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/controllers/observer/ActionObserver.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/items/fwd.h"
#include "aion/gameserver/skillengine/model/fwd.h"

namespace aion::gameserver::model::items {

/**
 * S0c declaration header (docs/design/hub-headers.md §3.5). Java `extends ActionObserver`, so the runtime base is ActionObserver's RefCounted
 * (ItemEquipmentListener adds it to the ObserveController, which holds `Ref<ActionObserver>`); Item holds it as `Field<Ref<ChargeInfo>>
 * conditioningInfo`, so it is no part and its destructor is protected (RefCounted rule).
 * The item back reference stays non-retaining (`OwnerRef<Item>`, S0c freeze decision): a retaining `Ref<Item>` would close the cycle
 * Item.conditioningInfo <-> ChargeInfo.item for every conditioned item, and no single Java lifecycle point could cut it (items become garbage
 * at many places). The plain reference is sound because the ChargeInfo is reachable only from its Item and, while the item is equipped, from
 * the owner's ObserveController: ItemEquipmentListener.onItemUnequipment removes the observer before an equipped item can leave Equipment
 * (delete, expiry, trade all unequip first), and LogoutBreakers step L7 clears the ObserveController at logout. A notification that took
 * the observer from the controller runs in a task scope that also keeps the Item from reclamation (design §2.4). Java nulls
 * Item.conditioningInfo (fusion break, Item.java:161-167) only for inventory items (ArmsfusionService.breakWeapons), never while the observer is
 * registered. Two checks keep the argument true for later ports (freeze review): lint L3 rejects a member or stored-lambda capture that names
 * ChargeInfo outside its holders (fieldmap.toml `holders`: Item.conditioningInfo; the ObserveController holds it as `Ref<ActionObserver>`),
 * and every body reads the item through getItem(), which terminates in checked builds when the Item was already destroyed (lint L3 rejects
 * other reads of `item`). The constructor reads the burn values from the item's improvement template.
 *
 * @author ATracer
 */
class ChargeInfo : public controllers::observer::ActionObserver {
	AION_MAKE_REF_FRIEND
public:
	static constexpr int32_t LEVEL2 = 1000000;
	static constexpr int32_t LEVEL1 = 500000;

private:
	const int32_t attackBurn;
	const int32_t defendBurn;
	// non-retaining back reference to the item that holds this object (lifetime argument in the class comment)
	runtime::OwnerRef<gameobjects::Item> item;
	runtime::Field<int32_t> chargePoints;
	runtime::Field<int32_t> playerId{};

protected:
	ChargeInfo(int32_t chargePoints, gameobjects::Item& item);
	~ChargeInfo() override;

public:
	/** Java: new ChargeInfo(chargePoints, item) */
	static runtime::Ref<ChargeInfo> create(int32_t chargePoints, gameobjects::Item& item);

	int32_t getChargePoints() const { return chargePoints.get(); }

private:
	/**
	 * C++ only: the item this object belongs to. Bodies read `item` only through this accessor (lint L3). Checked builds terminate (C4) when
	 * the Item was destroyed while this ChargeInfo is still referenced, instead of a use after free (class comment).
	 */
	gameobjects::Item& getItem() const;

	/** @return the player, null if none is set or the player is offline */
	runtime::Ptr<gameobjects::player::Player> getPlayer();

public:
	/** @param player null when the item is unequipped */
	void setPlayer(runtime::Ptr<gameobjects::player::Player> player);

	/**
	 * Updates the chargePoints of the item. (synchronized)
	 *
	 * @param pointsToAdd chargePoints to add to the current charge points
	 * @return boolean indicating whether the visual charge bar has changed or not
	 */
	bool updateChargePoints(int32_t pointsToAdd);

	void dotattacked(gameobjects::Creature& creature, skillengine::model::Effect& dotEffect) override;

	void attacked(gameobjects::Creature& creature, int32_t skillId) override;

	void attack(gameobjects::Creature& creature, int32_t skillId) override;

private:
	void sendItemUpdate();
};

} // namespace aion::gameserver::model::items
