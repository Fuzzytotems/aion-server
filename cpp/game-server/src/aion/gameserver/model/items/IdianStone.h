#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Parts.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/controllers/observer/fwd.h"
#include "aion/gameserver/model/gameobjects/Persistable_PersistentState.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/items/ItemStone.h"
#include "aion/gameserver/model/items/fwd.h"

namespace aion::gameserver::model::items {

/**
 * S0c declaration header (docs/design/hub-headers.md §3.5). A part of Item (`PartSlot<IdianStone, RetireTo::RECLAIMER>`, cycles review), bound to
 * the item in the constructor: `item` is the owner. retain()/release() of the ItemStone interfaces forward to OwnedPart (a Ref to the stone
 * retains the item, §9.2). The anonymous ActionObserver of onEquip is the fieldmap callback struct IdianStone_ActionObserver, defined in
 * IdianStone.cpp when onEquip is ported (§7.3). The constructor reads the item templates and creates the RandomBonusEffect (DataManager), so it
 * stays `AION_UNPORTED` (the ItemStone constructor already is).
 * C++ only: breakActionListener() cuts the stone -> observer -> player edge at logout without touching the effect or the observe controller
 * (LogoutBreakers, cycles review S0B-097).
 *
 * @author xTz
 */
class IdianStone : public runtime::OwnedPart, public ItemStone {
private:
	runtime::Field<runtime::Ref<controllers::observer::ActionObserver>> actionListener{};
	runtime::Field<int32_t> polishCharge;
	runtime::OwnerRef<gameobjects::Item> item;
	const int32_t burnDefend;
	const int32_t burnAttack;
	const runtime::Ref<RandomBonusEffect> rndBonusEffect;

public:
	IdianStone(int32_t itemId, PersistentState persistentState, gameobjects::Item& item, int32_t polishNumber, int32_t polishCharge);

	~IdianStone() override;

	/** C++ only: Ref<StatOwner> retains the owning item (hub-headers.md §9.2). */
	void retain() const noexcept override { runtime::OwnedPart::retain(); }

	void release() const noexcept override { runtime::OwnedPart::release(); }

	void onEquip(gameobjects::player::Player& player, int64_t slot);

private:
	void decreasePolishCharge(gameobjects::player::Player& player, bool isAttacked);

public:
	void decreasePolishCharge(gameobjects::player::Player& player, int32_t skillValue);

private:
	/** synchronized */
	void decreasePolishCharge(gameobjects::player::Player& player, bool isAttacked, int32_t skillValue);

public:
	int32_t getPolishNumber();

	int32_t getPolishCharge() const { return polishCharge.get(); }

	void onUnEquip(gameobjects::player::Player& player);

	/** C++ only (LogoutBreakers): actionListener = null without rndBonusEffect.endEffect or removeObserver */
	void breakActionListener() noexcept;
};

} // namespace aion::gameserver::model::items
