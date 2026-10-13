#include "aion/gameserver/handlers/ai/ArtifactAI.h"

#include <memory>
#include <string>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/ai/AIActions.h"
#include "aion/gameserver/ai/AIRequest.h"
#include "aion/gameserver/configs/main/LoggingConfig.h"
#include "aion/gameserver/controllers/ObserveController.h"
#include "aion/gameserver/controllers/PlayerController.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/SkillData.h"
#include "aion/gameserver/dataholders/loadingutils/EnumTraits.h"
#include "aion/gameserver/model/RaceInfo.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/siege/SiegeNpc.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/model/siege/ArtifactLocation.h"
#include "aion/gameserver/model/siege/SiegeRaceInfo.h"
#include "aion/gameserver/model/team/legion/Legion.h"
#include "aion/gameserver/model/team/legion/LegionMember.h"
#include "aion/gameserver/model/templates/siegelocation/ArtifactActivation.h"
#include "aion/gameserver/model/templates/siegelocation/SiegeLocationTemplate.h"
#include "aion/gameserver/model/templates/spawns/siegespawns/SiegeSpawnTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ABYSS_ARTIFACT_INFO3.h"
#include "aion/gameserver/network/aion/serverpackets/SM_EMOTION.h"
#include "aion/gameserver/network/aion/serverpackets/SM_QUESTION_WINDOW.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_USE_OBJECT.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/services/SiegeService.h"
#include "aion/gameserver/skillengine/SkillEngine.h"
#include "aion/gameserver/skillengine/model/Effect.h"
#include "aion/gameserver/skillengine/model/SkillTemplate.h"
#include "aion/gameserver/skillengine/properties/Properties.h"
#include "aion/gameserver/skillengine/properties/TargetRelationAttribute.h"
#include "aion/gameserver/utils/ChatUtil.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::handlers::ai {

AION_AI(ArtifactAI, "artifact");

const commons::logging::Logger ArtifactAI::log = commons::logging::LoggerFactory::getLogger("SIEGE_LOG");

/** Java: race.getL10n() of the L10n default method */
inline std::string artifactRaceL10n(model::Race race) {
	return ChatUtil::l10n(model::getL10nId(race));
}

/** Java: siegeRace.getL10n() of the L10n default method */
inline std::string artifactSiegeRaceL10n(model::siege::SiegeRace race) {
	return ChatUtil::l10n(model::siege::getL10nId(race));
}

/** Java: SiegeService.getArtifact(id), dereferenced (a NullPointerException for a siege id that is no artifact) */
inline ArtifactLocation& artifactOf(int32_t siegeId) {
	runtime::Ptr<ArtifactLocation> loc = SiegeService::getInstance().getArtifact(siegeId);
	if (!loc)
		throw runtime::NullPointerException("no artifact " + std::to_string(siegeId));
	return *loc;
}

/** Java: getTemplate().getActivation(), dereferenced */
inline const model::templates::siegelocation::ArtifactActivation& artifactActivationOf(ArtifactLocation& loc) {
	const model::templates::siegelocation::ArtifactActivation* activation = loc.getTemplate()->getActivation();
	if (activation == nullptr)
		throw runtime::NullPointerException("no artifact_activation for artifact " + std::to_string(loc.getLocationId()));
	return *activation;
}

/**
 * Java: the anonymous AIRequest of the confirmation window (ArtifactAI.java:65-70, fieldmap ai.ArtifactAI$2): the accepted activation. It
 * captures the AI (Java's enclosing instance).
 */
struct ArtifactAI_AIRequest_2 final : AIRequest {
	AION_MAKE_REF_FRIEND

	const runtime::Ref<ArtifactAI> artifactAI; // captured this ArtifactAI this (line 68)

	static runtime::Ref<ArtifactAI_AIRequest_2> create(ArtifactAI& artifactAI) { return runtime::makeRef<ArtifactAI_AIRequest_2>(artifactAI); }

	// Java ArtifactAI.java:66-69
	void acceptRequest(runtime::Ptr<Creature> requester, Player& responder, int32_t requestId) override {
		static_cast<void>(requester);
		static_cast<void>(requestId);
		artifactAI->onActivate(responder);
	}

protected:
	explicit ArtifactAI_AIRequest_2(ArtifactAI& artifactAIValue) : artifactAI(runtime::Ref<ArtifactAI>(artifactAIValue)) {}
	~ArtifactAI_AIRequest_2() override = default;
};

/**
 * Java: the anonymous AIRequest of the activation window (ArtifactAI.java:59-72, fieldmap ai.ArtifactAI$1): its acceptance opens the
 * confirmation window with the item cost. It captures the AI and the clicking player.
 */
struct ArtifactAI_AIRequest final : AIRequest {
	AION_MAKE_REF_FRIEND

	const runtime::Ref<ArtifactAI> artifactAI; // captured this ArtifactAI this (line 68)
	const runtime::Ref<Player> player;         // captured param Player player (line 64)

	static runtime::Ref<ArtifactAI_AIRequest> create(ArtifactAI& artifactAI, Player& player) {
		return runtime::makeRef<ArtifactAI_AIRequest>(artifactAI, player);
	}

	// Java ArtifactAI.java:61-71
	void acceptRequest(runtime::Ptr<Creature> requester, Player& responder, int32_t requestId) override {
		static_cast<void>(requester);
		static_cast<void>(responder);
		static_cast<void>(requestId);
		// show required item and count in confirm dialog
		runtime::Ref<ArtifactAI_AIRequest_2> confirm = ArtifactAI_AIRequest_2::create(*artifactAI);
		AIActions::addRequest(*artifactAI, *player, SM_QUESTION_WINDOW::STR_ASK_USE_ARTIFACT, *confirm, ChatUtil::l10n(716570),
			artifactActivationOf(artifactOf(artifactAI->getSpawnTemplate()->getSiegeId())).getCount());
	}

protected:
	ArtifactAI_AIRequest(ArtifactAI& artifactAIValue, Player& playerValue)
		: artifactAI(runtime::Ref<ArtifactAI>(artifactAIValue)), player(runtime::Ref<Player>(playerValue)) {}
	~ArtifactAI_AIRequest() override = default;
};

/**
 * Java: the anonymous ItemUseObserver of onActivate (ArtifactAI.java:132-153, fieldmap ai.ArtifactAI$4), attached to the activator until the
 * activation task removes it or a move aborts the channeling. It captures the AI, the activator, the artifact and its skill.
 */
struct ArtifactAI_ItemUseObserver final : ItemUseObserver {
	AION_MAKE_REF_FRIEND

	const runtime::Ref<ArtifactAI> artifactAI;  // captured this (line 137)
	const runtime::Ref<Player> player;          // captured param Player player (line 136)
	const runtime::Ref<ArtifactLocation> loc;   // captured local ArtifactLocation loc (line 139)
	const SkillTemplate* const skillTemplate;   // captured local SkillTemplate skillTemplate (line 139)

	static runtime::Ref<ArtifactAI_ItemUseObserver> create(ArtifactAI& artifactAI, Player& player, ArtifactLocation& loc,
		const SkillTemplate* skillTemplate) {
		return runtime::makeRef<ArtifactAI_ItemUseObserver>(artifactAI, player, loc, skillTemplate);
	}

	// Java ArtifactAI.java:134-152
	void abort() override {
		player->getController().cancelTask(TaskId::ACTION_ITEM_NPC);
		PacketSendUtility::broadcastPacket(*player, SM_EMOTION(*player, EmotionType::END_QUESTLOOT, 0, artifactAI->getObjectId()), true);
		PacketSendUtility::sendPacket(*player, SM_USE_OBJECT(player->getObjectId(), artifactAI->getObjectId(), 10000, 0));
		SM_SYSTEM_MESSAGE message = SM_SYSTEM_MESSAGE::STR_ARTIFACT_CANCELED(artifactSiegeRaceL10n(loc->getRace()), skillTemplate->getL10n());
		loc->setStatus(ArtifactStatus::IDLE);
		network::aion::serverpackets::SM_ABYSS_ARTIFACT_INFO3 artifactInfo(loc->getLocationId());
		artifactAI->getOwner().getPosition()->getWorldMapInstance()->forEachPlayer([&](Player& p) { // Java ArtifactAI$5 (confined)
			PacketSendUtility::sendPacket(p, message);
			PacketSendUtility::sendPacket(p, artifactInfo);
		});
	}

protected:
	ArtifactAI_ItemUseObserver(ArtifactAI& artifactAIValue, Player& playerValue, ArtifactLocation& locValue, const SkillTemplate* skillTemplateValue)
		: artifactAI(runtime::Ref<ArtifactAI>(artifactAIValue)), player(runtime::Ref<Player>(playerValue)), loc(runtime::Ref<ArtifactLocation>(locValue)),
		  skillTemplate(skillTemplateValue) {}
	~ArtifactAI_ItemUseObserver() override = default;
};

/**
 * Java: the inner class ArtifactUseSkill (ArtifactAI.java:201-268), the task that casts the artifact's skill: once, or every repeat interval
 * of a repeating artifact (scheduleAtFixedRate). runCount counts the runs; the task runs on one thread at a time (fixed rate never overlaps).
 */
struct ArtifactAI_ArtifactUseSkill final : runtime::RefCounted {
	AION_MAKE_REF_FRIEND

	const runtime::Ref<ArtifactAI> artifactAI; // Java: the enclosing instance (getOwner())
	const runtime::Ref<ArtifactLocation> artifact;
	const runtime::Ref<Player> player;
	const SkillTemplate* const skill;
	runtime::Field<int32_t> runCount{1};
	const std::shared_ptr<network::aion::serverpackets::SM_ABYSS_ARTIFACT_INFO3> pkt;
	const std::shared_ptr<SM_SYSTEM_MESSAGE> message;

	static runtime::Ref<ArtifactAI_ArtifactUseSkill> create(ArtifactAI& artifactAI, ArtifactLocation& artifact, Player& activator,
		const SkillTemplate* skill) {
		return runtime::makeRef<ArtifactAI_ArtifactUseSkill>(artifactAI, artifact, activator, skill);
	}

	// Java ArtifactAI.java:224-267
	void run() {
		if (artifact->getTemplate()->getRepeatCount() < runCount.get())
			return;
		const bool start = (runCount.get() == 1);
		const bool end = (runCount.get() == artifact->getTemplate()->getRepeatCount());
		runCount.set(runCount.get() + 1);
		if (start) {
			artifact->setStatus(ArtifactStatus::ACTIVATED);
		}
		artifactAI->getOwner().getPosition()->getWorldMapInstance()->forEachPlayer([&](Player& p) {
			if (start) {
				PacketSendUtility::sendPacket(p, *message);
				PacketSendUtility::sendPacket(p, *pkt);
			}
			if (end) {
				PacketSendUtility::sendPacket(p, *pkt);
			}
		});
		const bool pc = skill->getProperties()->getTargetSpecies() == TargetSpeciesAttribute::PC;
		artifact->forEachCreature([&](Creature& creature) {
			const runtime::Ptr<Creature> acting = creature.getActingCreature();
			if ((acting && dynamic_cast<Player*>(acting.get()) != nullptr) || (dynamic_cast<SiegeNpc*>(&creature) != nullptr && !pc)) {
				const std::optional<skillengine::properties::TargetRelationAttribute> relation = skill->getProperties()->getTargetRelation();
				if (!relation)
					throw runtime::NullPointerException("target_relation of skill " + std::to_string(skill->getSkillId())); // Java: switch (null)
				switch (*relation) {
					case skillengine::properties::TargetRelationAttribute::FRIEND:
						if (player->isEnemy(creature))
							return;
						break;
					case skillengine::properties::TargetRelationAttribute::ENEMY:
						if (!player->isEnemy(creature))
							return;
						break;
					default:
						break;
				}
				SkillEngine::getInstance().applyEffectDirectly(skill, skill->getLvl(), artifactAI->getOwner(), creature);
			}
		});
	}

protected:
	// Java ArtifactAI.java:216-222
	ArtifactAI_ArtifactUseSkill(ArtifactAI& artifactAIValue, ArtifactLocation& artifactValue, Player& activator, const SkillTemplate* skillValue)
		: artifactAI(runtime::Ref<ArtifactAI>(artifactAIValue)), artifact(runtime::Ref<ArtifactLocation>(artifactValue)),
		  player(runtime::Ref<Player>(activator)), skill(skillValue),
		  pkt(std::make_shared<network::aion::serverpackets::SM_ABYSS_ARTIFACT_INFO3>(artifactValue.getLocationId())),
		  message(std::make_shared<SM_SYSTEM_MESSAGE>(
			  SM_SYSTEM_MESSAGE::STR_ARTIFACT_FIRE(artifactRaceL10n(activator.getRace()), activator.getName(), skillValue->getL10n()))) {}
	~ArtifactAI_ArtifactUseSkill() override = default;
};

// Java ArtifactAI.java:50-53
runtime::Ptr<SiegeSpawnTemplate> ArtifactAI::getSpawnTemplate() {
	return runtime::cast<SiegeSpawnTemplate>(NpcAI::getSpawnTemplate());
}

// Java ArtifactAI.java:55-74
void ArtifactAI::handleDialogStart(Player& player) {
	ArtifactLocation& loc = artifactOf(getSpawnTemplate()->getSiegeId());
	// open artifact activation window
	runtime::Ref<ArtifactAI_AIRequest> request = ArtifactAI_AIRequest::create(*this, player);
	AIActions::addRequest(*this, player, SM_QUESTION_WINDOW::STR_ASK_ARTIFACT_POPUPDIALOG, loc.getCoolDown(), *request);
}

// Java ArtifactAI.java:76-78
void ArtifactAI::handleDialogFinish(Player& player) {
	static_cast<void>(player);
}

// Java ArtifactAI.java:80-199
void ArtifactAI::onActivate(Player& player) {
	ArtifactLocation& loc = artifactOf(getSpawnTemplate()->getSiegeId());

	// Get Skill id, item, count and target defined for each artifact.
	const model::templates::siegelocation::ArtifactActivation& activation = artifactActivationOf(loc);
	const int32_t skillId = activation.getSkillId();
	const int32_t itemId = activation.getItemId();
	const int32_t count = activation.getCount();
	const SkillTemplate* skillTemplate = dataholders::DataManager::SKILL_DATA->getSkillTemplate(skillId);

	if (skillTemplate == nullptr) {
		commons::logging::LoggerFactory::getLogger("ai.ArtifactAI").error("No skill template for artifact effect id : " + std::to_string(skillId));
		return;
	}

	if (loc.getCoolDown() > 0 || loc.getStatus() != ArtifactStatus::IDLE) {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_CANNOT_USE_ARTIFACT_OUT_OF_ORDER());
		return;
	}

	if (loc.getLegionId() != 0)
		if (!player.isLegionMember() || player.getLegion()->getLegionId() != loc.getLegionId()
			|| !player.getLegionMember()->hasRights(LegionPermissionsMask::ARTIFACT)) {
			PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_CANNOT_USE_ARTIFACT_HAVE_NO_AUTHORITY());
			return;
		}

	if (player.getInventory().getItemCountByItemId(itemId) < count)
		return;

	if (LoggingConfig::LOG_SIEGE.load())
		log.info("Artifact " + std::to_string(getSpawnTemplate()->getSiegeId()) + " activated by " + player.getName() + " (race: " +
			std::string(xml::enumName(player.getRace())) + ")");

	if (loc.getStatus() != ArtifactStatus::IDLE)
		return;

	// Brodcast start activation.
	SM_SYSTEM_MESSAGE startMessage = SM_SYSTEM_MESSAGE::STR_ARTIFACT_CASTING(artifactRaceL10n(player.getRace()), player.getName(), skillTemplate->getL10n());
	loc.setStatus(ArtifactStatus::ACTIVATION);
	network::aion::serverpackets::SM_ABYSS_ARTIFACT_INFO3 artifactInfo(loc.getLocationId());
	player.getPosition()->getWorldMapInstance()->forEachPlayer([&](Player& p) { // Java ArtifactAI$3 (confined)
		PacketSendUtility::sendPacket(p, startMessage);
		PacketSendUtility::sendPacket(p, artifactInfo);
	});

	PacketSendUtility::sendPacket(player, SM_USE_OBJECT(player.getObjectId(), getObjectId(), 10000, 1));
	PacketSendUtility::broadcastPacket(player, SM_EMOTION(player, EmotionType::START_QUESTLOOT, 0, getObjectId()), true);

	const runtime::Ref<ArtifactAI_ItemUseObserver> observer = ArtifactAI_ItemUseObserver::create(*this, player, loc, skillTemplate);
	player.getObserveController()->attach(*observer);
	// Java ArtifactAI$6 (ArtifactAI.java:155-197): a task pinning this, the player, the observer and the artifact
	ArtifactAI_ItemUseObserver& itemUseObserver = *observer;
	player.getController().addTask(TaskId::ACTION_ITEM_NPC,
		ThreadPoolManager::getInstance().schedule({this, &player, &itemUseObserver, &loc},
			[this, &player, &itemUseObserver, &loc, itemId, count, skillTemplate] {
				player.getObserveController()->removeObserver(itemUseObserver);
				PacketSendUtility::sendPacket(player, SM_USE_OBJECT(player.getObjectId(), getObjectId(), 10000, 0));
				PacketSendUtility::broadcastPacket(player, SM_EMOTION(player, EmotionType::END_QUESTLOOT, 0, getObjectId()), true);
				if (!player.getInventory().decreaseByItemId(itemId, count))
					return;
				SM_SYSTEM_MESSAGE message = SM_SYSTEM_MESSAGE::STR_ARTIFACT_CORE_CASTING(artifactSiegeRaceL10n(loc.getRace()), skillTemplate->getL10n());
				loc.setStatus(ArtifactStatus::CASTING);
				network::aion::serverpackets::SM_ABYSS_ARTIFACT_INFO3 coreInfo(loc.getLocationId());
				player.getPosition()->getWorldMapInstance()->forEachPlayer([&](Player& p) { // Java ArtifactAI$7 (confined)
					PacketSendUtility::sendPacket(p, message);
					PacketSendUtility::sendPacket(p, coreInfo);
				});

				loc.setLastActivation(commons::utils::currentTimeMillis());
				const runtime::Ref<ArtifactAI_ArtifactUseSkill> useSkill = ArtifactAI_ArtifactUseSkill::create(*this, loc, player, skillTemplate);
				ArtifactAI_ArtifactUseSkill& task = *useSkill;
				if (loc.getTemplate()->getRepeatCount() == 1)
					ThreadPoolManager::getInstance().schedule({&task}, [&task] { task.run(); }, 13000);
				else {
					const runtime::FutureRef s = ThreadPoolManager::getInstance().scheduleAtFixedRate({&task}, [&task] { task.run(); }, 13000,
						static_cast<int64_t>(loc.getTemplate()->getRepeatInterval()) * 1000);
					// Java ArtifactAI.java:188-193: cancels the repeating task and frees the artifact
					ThreadPoolManager::getInstance().schedule({&loc}, [s, &loc] {
						s->cancel(true);
						loc.setStatus(ArtifactStatus::IDLE);
					}, 13000 + (static_cast<int64_t>(loc.getTemplate()->getRepeatInterval()) * loc.getTemplate()->getRepeatCount() * 1000));
				}
			},
			10000));
}

} // namespace aion::gameserver::handlers::ai
