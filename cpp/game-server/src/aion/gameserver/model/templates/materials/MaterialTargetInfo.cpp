#include "aion/gameserver/model/templates/materials/MaterialTargetInfo.h"

#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/Summon.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"

namespace aion::gameserver::model::templates::materials {

bool matches(MaterialTarget target, gameobjects::Creature& creature) {
	switch (target) {
		case MaterialTarget::ALL:
			return true;
		case MaterialTarget::NPC:
			return static_cast<bool>(runtime::as<gameobjects::Npc>(creature));
		case MaterialTarget::PLAYER:
			return static_cast<bool>(runtime::as<gameobjects::player::Player>(creature));
		case MaterialTarget::PLAYER_WITH_PET: {
			if (matches(MaterialTarget::PLAYER, creature))
				return true;
			runtime::Ptr<gameobjects::Summon> summon = runtime::as<gameobjects::Summon>(creature);
			return summon && summon->getMaster();
		}
	}
	return false;
}

} // namespace aion::gameserver::model::templates::materials
