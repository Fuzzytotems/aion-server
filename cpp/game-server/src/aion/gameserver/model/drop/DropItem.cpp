#include "aion/gameserver/model/drop/DropItem.h"

#include <utility>

#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/model/drop/Drop.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/items/detail/StaticDataLookups.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/world/World.h"

namespace aion::gameserver::model::drop {

namespace {

/** Java: DataManager.ITEM_DATA.getItemTemplate(dropTemplate.getItemId()).getOptionSlotBonus() != 0 ? -1 : 0 (NullPointerException on nulls) */
int32_t optionalSocketOf(const Drop* dropTemplate) {
	if (dropTemplate == nullptr)
		throw runtime::NullPointerException("dropTemplate");
	const templates::item::ItemTemplate* itemTemplate = items::detail::getItemTemplate(dropTemplate->getItemId());
	if (itemTemplate == nullptr)
		throw runtime::NullPointerException("itemTemplate");
	return itemTemplate->getOptionSlotBonus() != 0 ? -1 : 0;
}

/** The owned run-time drop of RuntimeDropItem; a base class so that it is constructed before DropItem reads it (base-from-member) */
struct OwnedRuntimeDrop {
	const Drop drop;

	explicit OwnedRuntimeDrop(Drop&& value) : drop(std::move(value)) {}
};

/**
 * C++ only: the drop item of a run-time drop (DropItem::create(Drop&&)). Java's DropItem is the only reference to such a Drop, so the drop item
 * owns it; DropItem::dropTemplate points into this object, whose OwnedRuntimeDrop base is destroyed after the DropItem base.
 */
class RuntimeDropItem final : private OwnedRuntimeDrop, public DropItem {
	AION_MAKE_REF_FRIEND

protected:
	explicit RuntimeDropItem(Drop&& runtimeDrop) : OwnedRuntimeDrop(std::move(runtimeDrop)), DropItem(&this->drop) {}
	~RuntimeDropItem() override = default;
};

} // namespace

DropItem::DropItem(const Drop* value) : dropTemplate(value), optionalSocket(optionalSocketOf(value)) {
}

runtime::Ref<DropItem> DropItem::create(const Drop* value) {
	return runtime::makeRef<DropItem>(value);
}

runtime::Ref<DropItem> DropItem::create(Drop&& runtimeDrop) {
	return runtime::makeRef<RuntimeDropItem>(std::move(runtimeDrop));
}

void DropItem::calculateCount() {
	count.set(commons::utils::Rnd::get(dropTemplate->getMinAmount(), dropTemplate->getMaxAmount()));
}

bool DropItem::canViewDropItem(int32_t objId) {
	return playerObjIds.isEmpty() || playerObjIds.contains(objId);
}

void DropItem::setPlayerObjId(int32_t playerObjId) {
	if (playerObjId > 0 && !playerObjIds.contains(playerObjId)) // java-race: check-then-act as in Java
		this->playerObjIds.add(playerObjId);
}

void DropItem::setWinningPlayer(runtime::Ptr<gameobjects::player::Player> value) {
	this->winningPlayer.set(value);
}

runtime::Ptr<gameobjects::player::Player> DropItem::getWinningPlayer() {
	// java-race: winningPlayer is re-read like Java; a concurrent setWinningPlayer(null) between the reads throws NullPointerException
	if (winningPlayer.get()) {
		if (winningPlayer->isOnline()) {
			return winningPlayer.get();
		} else {
			runtime::Ptr<gameobjects::player::Player> player = world::World::getInstance().getPlayer(winningPlayer->getObjectId());
			if (player) {
				return player;
			} else {
				return winningPlayer.get();
			}
		}
	}
	return winningPlayer.get();
}

bool DropItem::isOnlyPossibleLooter(gameobjects::player::Player& player) {
	if (playerObjIds.size() != 1)
		return false;
	return playerObjIds.contains(player.getObjectId());
}

int32_t DropItem::getLootEffectId() {
	switch (dropTemplate->getItemId()) {
		case 166020000:
		case 166020001:
		case 166020002:
		case 166020003:
			return 1003; // Omega Enchantment Stone
		case 168000034:
		case 168000035:
		case 168000073:
		case 168000074:
		case 168000117:
		case 168000118:
		case 168000120:
		case 168000121:
		case 168000161:
		case 168000162:
		case 168000164:
		case 168000165:
		case 168000213:
		case 168000216:
		case 168000223:
		case 168000228:
		case 168000230:
		case 168000233:
		case 168000240:
		case 168000245:
			return 1003; // Godstones
		case 188053083:
			return 1003; // Tempering Solution Chest
		case 188053547:
		case 188053548:
		case 188053646:
		case 188053647:
			return 1002; // Nether Dragon King weapon boxes
		case 190100004:
		case 190100052:
			return 1003; // Mounts
		default:
			return 0;
	}
}

DropItem::~DropItem() = default;

} // namespace aion::gameserver::model::drop
