#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>

#include "aion/gameserver/controllers/fwd.h"
#include "aion/gameserver/model/fwd.h"
#include "aion/gameserver/model/gameobjects/SummonedObject.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/house/fwd.h"
#include "aion/gameserver/model/templates/spawns/fwd.h"

namespace aion::gameserver::model::gameobjects {

/**
 * A house npc (butler, relationship crystal, ...) created by its house (Java `SummonedObject<House>`, erased: hub-headers.md §8.1: getCreator()
 * returns the house as `Ptr<VisibleObject>`). A visible object: `VisibleObject::create<SummonedHouseNpc>(controller, spawnTemplate, house)`
 * (§10.1).
 * C++ only: getMasterName() keeps Java's "" for a house without owner name (Npc's `Field<std::string>` cannot tell "" from null).
 *
 * @author Rolandas
 */
class SummonedHouseNpc : public SummonedObject {
	AION_MAKE_REF_FRIEND
protected:
	SummonedHouseNpc(CreateKey key, std::unique_ptr<controllers::NpcController> controller, templates::spawns::SpawnTemplate& spawnTemplate,
		house::House& house);
	~SummonedHouseNpc() override;

public:
	int32_t getCreatorId() override;

	bool isEnemy(Creature& creature) override;

	/** C++: keeps SummonedObject::isEnemyFrom(Creature&) visible next to the overrides (no hiding in Java) */
	using SummonedObject::isEnemyFrom;

	bool isEnemyFrom(Npc& npc) override;

	bool isEnemyFrom(player::Player& player) override;

	CreatureType getType(Creature& creature) override;

	/** C++ only: Java's master name "" set by the constructor for a house without owner name (see the class comment) */
	std::optional<std::string> getMasterName() override;
};

} // namespace aion::gameserver::model::gameobjects
