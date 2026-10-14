#include "aion/gameserver/handlers/instance/HaramelInstance.h"

#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/controllers/attack/AggroList.h"
#include "aion/gameserver/model/PlayerClass.h"
#include "aion/gameserver/network/aion/serverpackets/SM_PLAY_MOVIE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"

namespace aion::gameserver::handlers::instance {

AION_INSTANCE_HANDLER(HaramelInstance, 300200000);

// Java HaramelInstance.java:18-20
HaramelInstance::HaramelInstance(WorldMapInstance& instance) : GeneralInstanceHandler(instance) {
}

runtime::Ref<HaramelInstance> HaramelInstance::create(WorldMapInstance& instance) {
	return runtime::makeRef<HaramelInstance>(instance);
}

// Java HaramelInstance.java:23-62
void HaramelInstance::onDie(Npc& npc) {
	switch (npc.getNpcId()) {
		case 216922: {
			runtime::Ptr<Player> player = npc.getAggroList().getMostPlayerDamage();
			npc.getController().delete_();
			if (player == nullptr)
				return;
			sendMsg(SM_SYSTEM_MESSAGE::STR_MSG_IDNOVICE_HAMEROON_TREASUREBOX_SPAWN());
			PacketSendUtility::sendPacket(*player, SM_PLAY_MOVIE(false, 0, 0, 457, true));
			switch (player->getPlayerClass()) {
				case model::PlayerClass::GLADIATOR:
				case model::PlayerClass::TEMPLAR:
					spawn(700829, 224.137f, 268.608f, 144.898f, int8_t{90}); // chest warrior
					break;
				case model::PlayerClass::ASSASSIN:
				case model::PlayerClass::RANGER:
				case model::PlayerClass::GUNNER:
					spawn(700830, 224.137f, 268.608f, 144.898f, int8_t{90}); // chest scout
					break;
				case model::PlayerClass::BARD:
				case model::PlayerClass::SORCERER:
				case model::PlayerClass::SPIRIT_MASTER:
					spawn(700831, 224.137f, 268.608f, 144.898f, int8_t{90}); // chest mage
					break;
				case model::PlayerClass::CLERIC:
				case model::PlayerClass::CHANTER:
				case model::PlayerClass::RIDER:
					spawn(700832, 224.137f, 268.608f, 144.898f, int8_t{90}); // chest cleric
					break;
				default:
					break;
			}
			spawn(700852, 224.5984f, 331.1431f, 141.8925f, int8_t{90}); // spawn opened dimensional gate
			break;
		}
		case 216920: // Brainwashed Dukaki Weakarm
		case 216921: // Brainwashed Dukaki Peon
		case 217067: // Brainwashed MuMu Worker
		case 700950: // Aether Cart
			npc.getController().delete_();
			break;
		default:
			break;
	}
}

} // namespace aion::gameserver::handlers::instance
