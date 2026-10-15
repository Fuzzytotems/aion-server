#include "aion/gameserver/model/items/ChargeInfo.h"

#include <algorithm>

#include "aion/gameserver/controllers/observer/ObserverType.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/Persistable.h"
#include "aion/gameserver/model/gameobjects/player/Equipment.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/templates/item/Improvement.h"
#include "aion/gameserver/network/aion/serverpackets/SM_INVENTORY_UPDATE_ITEM.h"
#include "aion/gameserver/runtime/base/Checked.h"
#include "aion/gameserver/runtime/sync/Monitor.h"
#include "aion/gameserver/services/item/ItemPacketService_ItemUpdateType.h"
#include "aion/gameserver/skillengine/model/Effect.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/world/World.h"

namespace aion::gameserver::model::items {

namespace {

/** Java: item.getImprovement() != null ? item.getImprovement().getBurnAttack() : 0 */
int32_t burnAttackOf(gameobjects::Item& item) {
	const templates::item::Improvement* improvement = item.getImprovement();
	return improvement != nullptr ? improvement->getBurnAttack() : 0;
}

/** Java: item.getImprovement() != null ? item.getImprovement().getBurnDefend() : 0 */
int32_t burnDefendOf(gameobjects::Item& item) {
	const templates::item::Improvement* improvement = item.getImprovement();
	return improvement != nullptr ? improvement->getBurnDefend() : 0;
}

/** Java int arithmetic (two's complement wrap-around) */
constexpr int32_t javaAdd(int32_t a, int32_t b) noexcept {
	return static_cast<int32_t>(static_cast<uint32_t>(a) + static_cast<uint32_t>(b));
}

constexpr int32_t javaNegate(int32_t a) noexcept {
	return static_cast<int32_t>(0u - static_cast<uint32_t>(a));
}

} // namespace

ChargeInfo::ChargeInfo(int32_t chargePointsValue, gameobjects::Item& itemValue)
	: ActionObserver(controllers::observer::ObserverType::DOT_ATTACK_DEFEND), attackBurn(burnAttackOf(itemValue)),
	  defendBurn(burnDefendOf(itemValue)), item(itemValue), chargePoints(chargePointsValue) {
}

ChargeInfo::~ChargeInfo() = default;

runtime::Ref<ChargeInfo> ChargeInfo::create(int32_t chargePointsValue, gameobjects::Item& itemValue) {
	return runtime::makeRef<ChargeInfo>(chargePointsValue, itemValue);
}

gameobjects::Item& ChargeInfo::getItem() const {
	AION_CHECK("C4", item.isManaged(), "ChargeInfo.item: the Item was destroyed while its ChargeInfo is still referenced (ChargeInfo.h)");
	return item;
}

runtime::Ptr<gameobjects::player::Player> ChargeInfo::getPlayer() {
	return playerId.get() == 0 ? nullptr : world::World::getInstance().getPlayer(playerId.get());
}

void ChargeInfo::setPlayer(runtime::Ptr<gameobjects::player::Player> player) {
	this->playerId.set(!player ? 0 : player->getObjectId());
}

bool ChargeInfo::updateChargePoints(int32_t pointsToAdd) {
	bool chargeBarStepChanged = false;
	SYNCHRONIZED(*this) {
		int32_t newChargePoints = javaAdd(chargePoints.get(), pointsToAdd);
		newChargePoints = std::max(0, std::min(newChargePoints, LEVEL2));
		int32_t currentChargeBarStep = chargePoints.get() / 50000;
		int32_t newChargeBarStep = newChargePoints / 50000;
		chargePoints.set(newChargePoints);
		runtime::Ptr<gameobjects::player::Player> player;
		if (getItem().isEquipped() && (player = getPlayer()))
			player->getEquipment().setPersistentState(gameobjects::Persistable::PersistentState::UPDATE_REQUIRED);
		getItem().setPersistentState(gameobjects::Persistable::PersistentState::UPDATE_REQUIRED);
		chargeBarStepChanged = currentChargeBarStep != newChargeBarStep;
	}
	return chargeBarStepChanged;
}

void ChargeInfo::dotattacked(gameobjects::Creature& /*creature*/, skillengine::model::Effect& /*dotEffect*/) {
	if (updateChargePoints(javaNegate(defendBurn)))
		sendItemUpdate();
}

void ChargeInfo::attacked(gameobjects::Creature& /*creature*/, int32_t skillId) {
	if (skillId == 0 && updateChargePoints(javaNegate(defendBurn)))
		sendItemUpdate();
}

void ChargeInfo::attack(gameobjects::Creature& /*creature*/, int32_t skillId) {
	if (skillId == 0 && updateChargePoints(javaNegate(attackBurn)))
		sendItemUpdate();
}

void ChargeInfo::sendItemUpdate() {
	runtime::Ptr<gameobjects::player::Player> player = getPlayer();
	if (player)
		utils::PacketSendUtility::sendPacket(*player, network::aion::serverpackets::SM_INVENTORY_UPDATE_ITEM(*player, getItem(),
			services::item::ItemPacketService_ItemUpdateType::CHARGE));
}

} // namespace aion::gameserver::model::items
