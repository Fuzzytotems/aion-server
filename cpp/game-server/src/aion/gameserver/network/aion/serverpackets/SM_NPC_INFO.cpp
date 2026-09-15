#include "aion/gameserver/network/aion/serverpackets/SM_NPC_INFO.h"

#include <string>

#include "aion/gameserver/controllers/movement/CreatureMoveController.h"
#include "aion/gameserver/model/CreatureTypeInfo.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/NpcObjectTypeInfo.h"
#include "aion/gameserver/model/gameobjects/Summon.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/items/NpcEquippedGear.h"
#include "aion/gameserver/model/templates/BoundRadius.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/model/templates/npc/NpcTemplate.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"
#include "aion/gameserver/network/aion/serverpackets/detail/PacketLookups.h"
#include "aion/gameserver/runtime/base/Exceptions.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_NPC_INFO::SM_NPC_INFO(model::gameobjects::Npc& npcValue, model::gameobjects::player::Player& player)
	: AionServerPacket(opcodeOf<SM_NPC_INFO>), npc(npcValue) {
	creatorId = npcValue.getCreatorId();
	masterName = npcValue.getMasterName().value_or(std::string()); // Java null: writeS writes only the terminator, like ""
	creatureType = npcValue.getType(player);
}

SM_NPC_INFO::SM_NPC_INFO(model::gameobjects::Summon& summon, model::gameobjects::player::Player& player)
	: AionServerPacket(opcodeOf<SM_NPC_INFO>), npc(summon) {
	// Java: Player owner = summon.getMaster() (the covariant override returns Player)
	runtime::Ptr<model::gameobjects::player::Player> owner = runtime::as<model::gameobjects::player::Player>(summon.getMaster());
	if (owner == nullptr)
		throw runtime::NullPointerException("SM_NPC_INFO: the summon has no master player");
	creatorId = owner->getObjectId();
	masterName = owner->getName();
	creatureType = summon.getType(player);
}

SM_NPC_INFO::~SM_NPC_INFO() = default;

void SM_NPC_INFO::writeImpl(AionConnection* con) {
	const auto* npcTemplate = dynamic_cast<const model::templates::npc::NpcTemplate*>(npc->getObjectTemplate());
	if (npcTemplate == nullptr) // Java: (NpcTemplate) npc.getObjectTemplate()
		throw runtime::ClassCastException("SM_NPC_INFO: the object template of " + std::to_string(npc->getObjectId()) + " is no NpcTemplate");
	runtime::Ptr<controllers::movement::CreatureMoveController> mc = npc->getMoveController();
	writeF(npc->getX());
	writeF(npc->getY());
	writeF(npc->getZ());
	writeD(npc->getObjectId());
	writeD(npcTemplate->getTemplateId()); // npc id reference for hp gauge + talk properties
	writeD(npcTemplate->getTemplateId()); // npc id reference for visual appearance
	writeC(model::getId(creatureType));
	/*
	 * 3,19 - wings spread
	 * 5,6,11,21 - sitting
	 * 7,23,71 - dead, no drop
	 * 8,24 - dead, looks like some orb of light (no normal mesh)
	 * 32,33 - fight mode
	 * 65 - normal
	 */
	writeH(npc->getState());
	writeC(npc->getHeading());
	writeD(npcTemplate->getL10nId());
	writeD(npcTemplate->getTitleId()); // TODO: implement fortress titles
	writeH(0x00); // unk
	writeC(0x00); // unk
	writeD(0x00); // unk
	/*
	 * Creator/Master Info (Summon, Kisk, Etc)
	 */
	writeD(creatorId); // creatorId - playerObjectId or House address
	writeS(masterName); // masterName
	writeC(detail::getHpPercentage(*npc));
	writeD(detail::getMaxHpCurrent(*npc));
	writeC(npc->getLevel());
	runtime::Ptr<model::items::NpcEquippedGear> gear = npc->getOverrideEquipment(); // dynamically overriden Equipment (only for NPCs, not summons)
	if (gear == nullptr) {
		writeD(0x00);
	} else {
		writeD(gear->getItemsMask());
		bool hasWeapon = false;
		// getting it from template (later if we make sure that npcs actually use items, we'll make Item from it)
		for (const auto& item : *gear) {
			if (!hasWeapon)
				hasWeapon = item.value->isWeapon();
			writeD(item.value->getTemplateId());
			writeD(0x00);
			writeD(0x00);
			writeH(0x00);
			writeH(0x00); // 4.7
		}
	}
	writeF(npcTemplate->getBoundRadius()->getMaxOfFrontAndSide());
	writeF(npcTemplate->getHeight());
	writeF(detail::getMovementSpeedFloat(*npc)); // speed
	writeH(npcTemplate->getAttackSpeed());
	writeH(npcTemplate->getAttackSpeed());
	writeC(npc->isFlag() ? 0x13 : npc->isNewSpawn() ? 0x01 : 0x00);
	writeF(mc->getTargetX2());
	writeF(mc->getTargetY2());
	writeF(mc->getTargetZ2());
	writeC(mc->getMovementMask()); // move type
	runtime::Ptr<model::templates::spawns::SpawnTemplate> spawn = npc->getSpawn();
	writeH(spawn == nullptr ? 0 : spawn->getStaticId());
	writeC(0);
	writeC(0); // all unknown
	writeC(0);
	writeC(0);
	writeC(0);
	writeC(0);
	writeC(0);
	writeC(0);
	writeC(npc->getVisualState()); // visualState
	writeH(model::gameobjects::getId(npc->getNpcObjectType()));
	writeC(0x00); // unk
	runtime::Ptr<model::gameobjects::VisibleObject> target = npc->getTarget();
	writeD(target == nullptr ? 0 : target->getObjectId());
	writeD(detail::getTownIdByPosition(*npc));
	writeD(0); // unk 4.7.5
}

} // namespace aion::gameserver::network::aion::serverpackets
