#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/collections/ArrayList.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/model/drop/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"

namespace aion::gameserver::model::drop {

/**
 * S0c declaration header (docs/design/hub-headers.md §3.5), written for the member-type guards waiting for it at the spine freeze.
 *
 * @author ATracer
 */
class DropItem : public runtime::RefCounted {
	AION_MAKE_REF_FRIEND
private:
	runtime::Field<int32_t> index{0};
	runtime::Field<int64_t> count{0};
	const Drop* dropTemplate;
	runtime::ArrayList<int32_t> playerObjIds{AION_LOCK_CLASS(DropItem::playerObjIds)};
	runtime::Field<bool> isFreeForAll_{false};
	runtime::Field<int64_t> highestValue{0};
	runtime::Field<runtime::Ref<gameobjects::player::Player>> winningPlayer{};
	runtime::Field<bool> isItemWonNotCollected_{false};
	runtime::Field<bool> isDistributeItem_{false};
	runtime::Field<int32_t> npcObj{};
	const int32_t optionalSocket; // Java: = 0

protected:
	explicit DropItem(const Drop* dropTemplate);

public:
	static runtime::Ref<DropItem> create(const Drop* value);

	/** Regenerates item count upon each call */
	void calculateCount();

	int32_t getIndex() const { return this->index.get(); }

	void setIndex(int32_t value) { this->index.set(value); }

	int64_t getCount() const { return this->count.get(); }

	void setCount(int64_t value) { this->count.set(value); }

	const Drop* getDropTemplate() const { return this->dropTemplate; }

	runtime::ArrayList<int32_t>& getPlayerObjIds() { return this->playerObjIds; }

	bool canViewDropItem(int32_t objId);

	void setPlayerObjId(int32_t playerObjId);

	void isFreeForAll(bool value) { this->isFreeForAll_.set(value); }

	bool isFreeForAll() const { return this->isFreeForAll_.get(); }

	int64_t getHighestValue() const { return this->highestValue.get(); }

	void setHighestValue(int64_t value) { this->highestValue.set(value); }

	void setWinningPlayer(runtime::Ptr<gameobjects::player::Player> winningPlayer);

	runtime::Ptr<gameobjects::player::Player> getWinningPlayer();

	void isItemWonNotCollected(bool value) { this->isItemWonNotCollected_.set(value); }

	bool isItemWonNotCollected() const { return this->isItemWonNotCollected_.get(); }

	void isDistributeItem(bool value) { this->isDistributeItem_.set(value); }

	bool isDistributeItem() const { return this->isDistributeItem_.get(); }

	int32_t getNpcObj() const { return this->npcObj.get(); }

	void setNpcObj(int32_t value) { this->npcObj.set(value); }

	int32_t getOptionalSocket() const { return this->optionalSocket; }

	bool isOnlyPossibleLooter(gameobjects::player::Player& player);

	int32_t getLootEffectId();

protected:
	~DropItem() override;
};

} // namespace aion::gameserver::model::drop
