#include "aion/gameserver/model/items/IdianStone.h"

#include "aion/gameserver/controllers/ObserveController.h"
#include "aion/gameserver/controllers/observer/ActionObserver.h"
#include "aion/gameserver/controllers/observer/ObserverType.h"
#include "aion/gameserver/dao/ItemStoneListDAO.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/items/ItemSlotInfo.h"
#include "aion/gameserver/model/items/RandomBonusEffect.h"
#include "aion/gameserver/model/items/detail/StaticDataLookups.h"
#include "aion/gameserver/model/templates/item/Idian.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/model/templates/item/actions/ItemActions.h"
#include "aion/gameserver/model/templates/item/actions/PolishAction.h"
#include "aion/gameserver/model/templates/item/bonuses/StatBonusType.h"
#include "aion/gameserver/network/aion/serverpackets/SM_INVENTORY_UPDATE_ITEM.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/sync/Monitor.h"
#include "aion/gameserver/services/item/ItemPacketService_ItemUpdateType.h"
#include "aion/gameserver/skillengine/model/Effect.h"
#include "aion/gameserver/utils/PacketSendUtility.h"

namespace aion::gameserver::model::items {

namespace {

/** Java `item.getItemTemplate().getIdianAction()`, dereferenced by the constructor (NullPointerException without an idian action) */
const templates::item::Idian& idianActionOf(gameobjects::Item& item) {
	const templates::item::Idian* idianAction = item.getItemTemplate()->getIdianAction();
	if (idianAction == nullptr)
		throw runtime::NullPointerException("idianAction");
	return *idianAction;
}

/** Java `DataManager.ITEM_DATA.getItemTemplate(itemId).getActions().getPolishAction().getPolishSetId()` (NullPointerException on each null) */
int32_t polishSetIdOf(int32_t itemId) {
	const templates::item::ItemTemplate* itemTemplate = detail::getItemTemplate(itemId);
	if (itemTemplate == nullptr)
		throw runtime::NullPointerException("itemTemplate");
	const templates::item::actions::ItemActions* actions = itemTemplate->getActions();
	if (actions == nullptr)
		throw runtime::NullPointerException("actions");
	const templates::item::actions::PolishAction* polishAction = detail::getPolishAction(*actions);
	if (polishAction == nullptr)
		throw runtime::NullPointerException("polishAction");
	return polishAction->getPolishSetId();
}

/** Java int subtraction and addition (two's complement wrap-around) */
constexpr int32_t javaSub(int32_t a, int32_t b) noexcept {
	return static_cast<int32_t>(static_cast<uint32_t>(a) - static_cast<uint32_t>(b));
}

constexpr int32_t javaAdd(int32_t a, int32_t b) noexcept {
	return static_cast<int32_t>(static_cast<uint32_t>(a) + static_cast<uint32_t>(b));
}

} // namespace

/**
 * Java: the anonymous ActionObserver of IdianStone.onEquip (fieldmap.py --class 'com.aionemu.gameserver.model.items.IdianStone$1'), stored in
 * actionListener and in the player's ObserveController. The stone is its storing object, captured non-retaining (fieldmap.toml [captures],
 * header request items-7); onUnEquip and the logout breaker (breakActionListener) drop the observer before the stone can go away.
 * <p>
 * C++ only: every callback reads the stone through stone(). Checked builds terminate there (C4) when the stone's Item was already destroyed
 * while the observer is still registered (an item destroyed without unequipping it), like ChargeInfo::getItem. A stone replaced in its Item
 * without onUnEquip cannot be detected: a part has no liveness cookie, and its destructor may not reach the observer (C8).
 */
struct IdianStone_ActionObserver final : controllers::observer::ActionObserver {
	AION_MAKE_REF_FRIEND

	// fieldmap.toml: captured this IdianStone this (line 45), a non-retaining pointer to the storing stone (header request items-7)
	IdianStone* const idianStone;
	const runtime::Ref<gameobjects::player::Player> player; // captured param Player player (line 45)
	// lint: L3 C++ only: the stone's owning Item (the stone is a part of it), non-retaining like idianStone; stone() only reads its liveness cookie
	gameobjects::Item& owningItem;

	static runtime::Ref<IdianStone_ActionObserver> create(IdianStone& idianStone, gameobjects::player::Player& player, gameobjects::Item& item) {
		return runtime::makeRef<IdianStone_ActionObserver>(idianStone, player, item);
	}

	void dotattacked(gameobjects::Creature& /*creature*/, skillengine::model::Effect& /*dotEffect*/) override {
		stone().decreasePolishCharge(*player, true);
	}

	void attacked(gameobjects::Creature& /*creature*/, int32_t /*skillId*/) override {
		stone().decreasePolishCharge(*player, true);
	}

	void attack(gameobjects::Creature& /*creature*/, int32_t skillId) override {
		if (skillId == 0)
			stone().decreasePolishCharge(*player, false);
	}

private:
	/** C++ only: the captured stone; checked builds terminate (C4) when its Item was destroyed while this observer is still referenced */
	IdianStone& stone() const {
		AION_CHECK("C4", owningItem.isManaged(),
			"IdianStone observer: the stone's Item was destroyed while the observer is still registered (IdianStone.onUnEquip was not called)");
		return *idianStone;
	}

protected:
	IdianStone_ActionObserver(IdianStone& idianStoneValue, gameobjects::player::Player& playerValue, gameobjects::Item& itemValue)
		: ActionObserver(controllers::observer::ObserverType::DOT_ATTACK_DEFEND), idianStone(&idianStoneValue),
		  player(runtime::Ref<gameobjects::player::Player>(playerValue)), owningItem(itemValue) {}
	~IdianStone_ActionObserver() override = default;
};

IdianStone::IdianStone(int32_t itemIdValue, PersistentState persistentStateValue, gameobjects::Item& itemValue, int32_t polishNumber,
	int32_t polishChargeValue)
	: OwnedPart(itemValue), ItemStone(itemValue.getObjectId(), itemIdValue, 0, persistentStateValue), polishCharge(polishChargeValue),
	  item(itemValue), burnDefend(idianActionOf(itemValue).getBurnDefend()), burnAttack(idianActionOf(itemValue).getBurnAttack()),
	  rndBonusEffect(RandomBonusEffect::create(templates::item::bonuses::StatBonusType::POLISH, polishSetIdOf(itemIdValue), polishNumber)) {
}

IdianStone::~IdianStone() = default;

void IdianStone::onEquip(gameobjects::player::Player& player, int64_t slotValue) {
	if (polishCharge.get() > 0 && (slotValue & getSlotIdMask(ItemSlot::MAIN_HAND)) != 0) {
		actionListener.set(IdianStone_ActionObserver::create(*this, player, item));
		player.getObserveController()->addObserver(*actionListener.get());
		rndBonusEffect->applyEffect(player);
	}
}

void IdianStone::decreasePolishCharge(gameobjects::player::Player& player, bool isAttacked) {
	decreasePolishCharge(player, isAttacked, 0);
}

void IdianStone::decreasePolishCharge(gameobjects::player::Player& player, int32_t skillValue) {
	decreasePolishCharge(player, false, skillValue);
}

void IdianStone::decreasePolishCharge(gameobjects::player::Player& player, bool isAttacked, int32_t skillValue) {
	SYNCHRONIZED(*this) {
		int32_t result;
		if (polishCharge.get() <= 0) {
			return;
		}
		if (skillValue == 0)
			result = isAttacked ? burnDefend : burnAttack;
		else
			result = skillValue;
		if (javaSub(polishCharge.get(), result) < 0) {
			polishCharge.set(0);
		} else {
			polishCharge.set(javaSub(polishCharge.get(), result));
		}
		if (polishCharge.get() <= 300000 && javaAdd(polishCharge.get(), result) > 300000) { // we just dropped to or below 300k
			utils::PacketSendUtility::sendPacket(player,
				network::aion::serverpackets::SM_INVENTORY_UPDATE_ITEM(player, item, services::item::ItemPacketService_ItemUpdateType::POLISH_CHARGE));
		} else if (polishCharge.get() < 0) {
			polishCharge.set(0);
		}
		if (polishCharge.get() == 0) {
			onUnEquip(player);
			utils::PacketSendUtility::sendPacket(player, network::aion::serverpackets::SM_INVENTORY_UPDATE_ITEM(player, item));
			item.setIdianStone(nullptr); // retires this part to the Reclaimer: it stays valid until the task ends
			setPersistentState(PersistentState::DELETED);
			// lockdep: Java stores the deleted stone inside synchronized decreasePolishCharge (IdianStone.java:98)
			dao::ItemStoneListDAO::storeIdianStones(*this);
		}
	}
}

int32_t IdianStone::getPolishNumber() {
	return rndBonusEffect->getStatBonusId();
}

void IdianStone::onUnEquip(gameobjects::player::Player& player) {
	if (actionListener.get()) { // java-race: check-then-act on actionListener as in Java
		rndBonusEffect->endEffect(player);
		player.getObserveController()->removeObserver(*actionListener.get());
		actionListener.set(nullptr);
	}
}

void IdianStone::breakActionListener() noexcept {
	actionListener.set(nullptr);
}

runtime::Ptr<controllers::observer::ActionObserver> IdianStone::getActionListener() const {
	return actionListener.get();
}

} // namespace aion::gameserver::model::items
