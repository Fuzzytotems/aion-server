#pragma once

#include <cstdint>
#include <memory>
#include <optional>

#include "aion/gameserver/runtime/collections/ArrayList.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/commons/database/SqlTypes.h"
#include "aion/gameserver/model/fwd.h"
#include "aion/gameserver/model/gameobjects/Persistable.h"
#include "aion/gameserver/model/gameobjects/Persistable_PersistentState.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/templates/L10n.h"
#include "aion/gameserver/model/town/fwd.h"

namespace aion::gameserver::model::town {

/**
 * S0c declaration header (docs/design/hub-headers.md §3.5), written for the member-type guards waiting for it at the spine freeze.
 *
 * @author ViAl
 */
class Town : public runtime::RefCounted, public gameobjects::Persistable, public templates::L10n {
	AION_MAKE_REF_FRIEND
private:
	const int32_t id;
	runtime::Field<int32_t> level{};
	runtime::Field<int32_t> points{};
	const commons::database::Timestamp levelUpDate;
	const Race race;
	runtime::Field<gameobjects::Persistable::PersistentState> persistentState{};
	runtime::ArrayList<runtime::Ref<gameobjects::Npc>> spawnedNpcs{AION_LOCK_CLASS(Town::spawnedNpcs)};

protected:
	/** Used only from DAO. */
	Town(int32_t id, int32_t level, int32_t points, Race race, std::optional<commons::database::Timestamp> levelUpDate);

public:
	static runtime::Ref<Town> create(int32_t value, int32_t levelValue, int32_t pointsValue, Race raceValue,
		std::optional<commons::database::Timestamp> levelUpDateValue);

protected:
	/** Used for initial import from house templates. */
	Town(int32_t id, Race race);

public:
	static runtime::Ref<Town> create(int32_t value, Race raceValue);

	int32_t getId() const { return this->id; }

	int32_t getL10nId() const override;

	int32_t getLevel() const { return this->level.get(); }

	int32_t getPoints() const { return this->points.get(); }

	void increasePoints(int32_t amount); // synchronized

private:
	void increaseLevel();

	void broadcastUpdate();

	void spawnNewObjects();

	void despawnOldObjects();

public:
	Race getRace() const { return this->race; }

	std::optional<commons::database::Timestamp> getLevelUpDate() const { return this->levelUpDate; }

	gameobjects::Persistable::PersistentState getPersistentState() override { return this->persistentState.get(); }

	void setPersistentState(gameobjects::Persistable::PersistentState state) override;

protected:
	~Town() override;
};

} // namespace aion::gameserver::model::town
