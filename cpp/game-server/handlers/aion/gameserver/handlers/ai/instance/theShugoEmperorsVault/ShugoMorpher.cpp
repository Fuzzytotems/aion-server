#include "aion/gameserver/handlers/ai/instance/theShugoEmperorsVault/ShugoMorpher.h"

#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/controllers/ObserveController.h"
#include "aion/gameserver/controllers/PlayerController.h"
#include "aion/gameserver/controllers/observer/ItemUseObserver.h"
#include "aion/gameserver/network/aion/serverpackets/SM_EMOTION.h"
#include "aion/gameserver/network/aion/serverpackets/SM_USE_OBJECT.h"
#include "aion/gameserver/services/DialogService.h"
#include "aion/gameserver/skillengine/SkillEngine.h"
#include "aion/gameserver/skillengine/model/Skill.h"

namespace aion::gameserver::handlers::ai::instance::theShugoEmperorsVault {

AION_AI(ShugoMorpher, "emperorsVaultMorphNPC");

/**
 * Java: the anonymous ItemUseObserver of handleDialogStart (ShugoMorpher.java:44-55), stored in the player's ObserveController until the use
 * task or abort() removes it (a logout's ObserveController::clearWithoutNotify drops it). It captures the AI and the player.
 */
struct ShugoMorpher_ItemUseObserver final : ItemUseObserver {
	AION_MAKE_REF_FRIEND

	const runtime::Ref<ShugoMorpher> shugoMorpher; // captured this
	const runtime::Ref<Player> player;             // captured param Player player

	static runtime::Ref<ShugoMorpher_ItemUseObserver> create(ShugoMorpher& shugoMorpher, Player& player) {
		return runtime::makeRef<ShugoMorpher_ItemUseObserver>(shugoMorpher, player);
	}

	// Java ShugoMorpher.java:46-53
	void abort() override {
		shugoMorpher->started.set(false);
		player->getObserveController()->removeObserver(*this);
		player->getController().cancelTask(TaskId::ACTION_ITEM_NPC);
		PacketSendUtility::broadcastPacket(*player, SM_EMOTION(*player, EmotionType::END_QUESTLOOT, 0, shugoMorpher->getObjectId()), true);
		PacketSendUtility::sendPacket(*player, SM_USE_OBJECT(player->getObjectId(), shugoMorpher->getObjectId(), 0, 2));
	}

protected:
	ShugoMorpher_ItemUseObserver(ShugoMorpher& shugoMorpherValue, Player& playerValue)
		: shugoMorpher(runtime::Ref<ShugoMorpher>(shugoMorpherValue)), player(runtime::Ref<Player>(playerValue)) {}
	~ShugoMorpher_ItemUseObserver() override = default;
};

// Java ShugoMorpher.java:36-39
void ShugoMorpher::handleAttack(runtime::Ptr<Creature> /*creature*/) {
	// do nothing
}

// Java ShugoMorpher.java:41-72 (the anonymous Runnable pins this, the player and the observer)
void ShugoMorpher::handleDialogStart(Player& player) {
	if (DialogService::isInteractionAllowed(player, getOwner()) && started.compareAndSet(false, true)) {
		const int32_t delay = 1000;
		const runtime::Ref<ShugoMorpher_ItemUseObserver> obs = ShugoMorpher_ItemUseObserver::create(*this, player);

		player.getObserveController()->attach(*obs);
		PacketSendUtility::sendPacket(player, SM_USE_OBJECT(player.getObjectId(), getObjectId(), delay, 1));
		PacketSendUtility::broadcastPacket(player, SM_EMOTION(player, EmotionType::START_QUESTLOOT, 0, getObjectId()), true);
		ShugoMorpher_ItemUseObserver& observer = *obs;
		player.getController().addTask(TaskId::ACTION_ITEM_NPC, ThreadPoolManager::getInstance().schedule({this, &player, &observer},
			[this, &player, &observer, delay] {
				PacketSendUtility::broadcastPacket(player, SM_EMOTION(player, EmotionType::END_QUESTLOOT, 0, getObjectId()), true);
				PacketSendUtility::sendPacket(player, SM_USE_OBJECT(player.getObjectId(), getObjectId(), delay, 2));
				player.getObserveController()->removeObserver(observer);
				morph(player);
			},
			delay));
	}
}

/** Java: ShugoMorpher.handleUseItemFinish( (ShugoMorpher.java:74-88) */
void ShugoMorpher::morph(Player& player) {
	Race race = player.getRace();
	bool morphed;
	switch (getOwner().getNpcId()) {
		case 833491:
		case 833494:
		case 832935:
			morphed = SkillEngine::getInstance().getSkill(getOwner(), race == Race::ELYOS ? 21829 : 21832, 1, runtime::Ptr<VisibleObject>(player))->useSkill();
			break;
		case 833492:
		case 833495:
		case 832936:
			morphed = SkillEngine::getInstance().getSkill(getOwner(), race == Race::ELYOS ? 21830 : 21833, 1, runtime::Ptr<VisibleObject>(player))->useSkill();
			break;
		case 833493:
		case 833496:
		case 832937:
			morphed = SkillEngine::getInstance().getSkill(getOwner(), race == Race::ELYOS ? 21831 : 21834, 1, runtime::Ptr<VisibleObject>(player))->useSkill();
			break;
		default:
			morphed = false;
			break;
	}

	if (morphed) {
		getOwner().getController().delete_();
	} else {
		started.set(false);
	}
}

// Java ShugoMorpher.java:90-93
float ShugoMorpher::modifyDamage(Creature& /*attacker*/, float /*damage*/, runtime::Ptr<Effect> /*effect*/) {
	return 0;
}

// Java ShugoMorpher.java:95-101
bool ShugoMorpher::ask(AIQuestion question) {
	switch (question) {
		case AIQuestion::REWARD_AP_XP_DP_LOOT:
		case AIQuestion::REWARD_LOOT:
			return false;
		default:
			return GeneralNpcAI::ask(question);
	}
}

} // namespace aion::gameserver::handlers::ai::instance::theShugoEmperorsVault
