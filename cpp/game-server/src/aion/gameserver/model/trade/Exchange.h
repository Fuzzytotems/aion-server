#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/collections/HashMap.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/trade/fwd.h"

namespace aion::gameserver::model::trade {

/**
 * S0c declaration header (docs/design/hub-headers.md §3.5), written for the member-type guards waiting for it at the spine freeze.
 *
 * @author ATracer
 */
class Exchange : public runtime::RefCounted {
	AION_MAKE_REF_FRIEND
private:
	const runtime::Ref<gameobjects::player::Player> activeplayer;
	const runtime::Ref<gameobjects::player::Player> targetPlayer;
	runtime::Field<bool> confirmed{};
	runtime::Field<bool> locked{};
	runtime::Field<int64_t> kinahCount{};
	runtime::HashMap<int32_t, runtime::Ref<ExchangeItem>> items{AION_LOCK_CLASS(Exchange::items)}; // Java: = new HashMap<>()

protected:
	Exchange(gameobjects::player::Player& activeplayer, gameobjects::player::Player& targetPlayer);

public:
	static runtime::Ref<Exchange> create(gameobjects::player::Player& value, gameobjects::player::Player& targetPlayerValue);

	void confirm();

	bool isConfirmed() const { return this->confirmed.get(); }

	void lock();

	bool isLocked() const { return this->locked.get(); }

	void addItem(int32_t parentItemObjId, ExchangeItem& exchangeItem);

	void addKinah(int64_t countToAdd);

	runtime::Ptr<gameobjects::player::Player> getActiveplayer() const { return this->activeplayer; }

	runtime::Ptr<gameobjects::player::Player> getTargetPlayer() const { return this->targetPlayer; }

	int64_t getKinahCount() const { return this->kinahCount.get(); }

	runtime::HashMap<int32_t, runtime::Ref<ExchangeItem>>& getItems() { return this->items; }

	bool isExchangeListFull();

protected:
	~Exchange() override;
};

} // namespace aion::gameserver::model::trade
