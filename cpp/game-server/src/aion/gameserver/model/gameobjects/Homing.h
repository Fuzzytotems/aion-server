#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>

#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/controllers/fwd.h"
#include "aion/gameserver/model/gameobjects/SummonedObject.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/templates/item/fwd.h"
#include "aion/gameserver/model/templates/spawns/fwd.h"

namespace aion::gameserver::model::gameobjects {

/**
 * A homing summoned by a skill of its creator (Java `SummonedObject<Creature>`, erased: hub-headers.md §8.1). A visible object:
 * `VisibleObject::create<Homing>(controller, spawnTemplate, level, creator, skillId)` (§10.1). The SummonedObject base initializer reads the NPC
 * template (DataManager, not ported yet), so the constructor reaches `AION_UNPORTED`.
 * C++ only: getMasterName() keeps Java's "" (the constructor sets an empty master name, which Npc's `Field<std::string>` cannot tell from null).
 *
 * @author ATracer
 */
class Homing : public SummonedObject {
	AION_MAKE_REF_FRIEND
private:
	const int32_t skillId;
	const templates::item::ItemAttackType attackType;
	/** Number of performed attacks */
	runtime::Field<int32_t> attackCount{};

protected:
	Homing(CreateKey key, std::unique_ptr<controllers::NpcController> controller, templates::spawns::SpawnTemplate& spawnTemplate, int8_t level,
		Creature& creator, int32_t skillId);
	~Homing() override;

	void setupStatContainers() override;

public:
	/** @param attackCount the attackCount to set */
	void setAttackCount(int32_t value) { attackCount.set(value); }

	/** @return the attackCount */
	int32_t getAttackCount() const { return attackCount.get(); }

	/** @return NpcObjectType.HOMING */
	NpcObjectType getNpcObjectType() override;

	templates::item::ItemAttackType getAttackType() override;

	int32_t getSkillId() const { return skillId; }

	/** C++ only: Java's master name "" set by the constructor (see the class comment) */
	std::optional<std::string> getMasterName() override;

private:
	templates::item::ItemAttackType findAttackType();
};

} // namespace aion::gameserver::model::gameobjects
