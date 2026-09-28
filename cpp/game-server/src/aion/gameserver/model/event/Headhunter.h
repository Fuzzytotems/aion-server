#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/model/event/fwd.h"
#include "aion/gameserver/model/gameobjects/Persistable.h"
#include "aion/gameserver/model/gameobjects/Persistable_PersistentState.h"

namespace aion::gameserver::model::event {

/**
 * Created on 30.05.2016
 *
 * Java implements Comparable<Headhunter>.
 *
 * @author Estrayl
 * @since AION 4.8
 */
class Headhunter : public runtime::RefCounted, public gameobjects::Persistable {
	AION_MAKE_REF_FRIEND
private:
	runtime::Field<gameobjects::Persistable::PersistentState> state{};
	const int32_t hunterId;
	runtime::Field<int32_t> accumulatedKills{};
	runtime::Field<int64_t> lastUpdate{};

protected:
	Headhunter(int32_t hunterId, int32_t accumulatedKills, int64_t lastUpdate, gameobjects::Persistable::PersistentState state);

public:
	static runtime::Ref<Headhunter> create(int32_t value, int32_t accumulatedKillsValue, int64_t lastUpdateValue,
		gameobjects::Persistable::PersistentState stateValue);

	int32_t getHunterId() const { return this->hunterId; }

	int32_t getKills() const { return this->accumulatedKills.get(); }

	void setKills(int32_t value) { this->accumulatedKills.set(value); }

	int32_t incrementAndGetKills();

	gameobjects::Persistable::PersistentState getPersistentState() override { return this->state.get(); }

	void setPersistentState(gameobjects::Persistable::PersistentState state) override;

	int64_t getLastUpdate() const { return this->lastUpdate.get(); }

	int32_t compareTo(const Headhunter& hunter) const;

protected:
	~Headhunter() override;
};

} // namespace aion::gameserver::model::event
