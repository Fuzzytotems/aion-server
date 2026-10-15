#include "aion/gameserver/services/siege/FortressSiege.h"

#include <algorithm>
#include <cmath>
#include <set>
#include <string>
#include <vector>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/utils/Rnd.h"
#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/configs/main/LoggingConfig.h"
#include "aion/gameserver/configs/main/SiegeConfig.h"
#include "aion/gameserver/dao/SiegeDAO.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/SkillData.h"
#include "aion/gameserver/dataholders/loadingutils/EnumTraits.h"
#include "aion/gameserver/model/RaceInfo.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"
#include "aion/gameserver/model/gameobjects/siege/SiegeNpc.h"
#include "aion/gameserver/model/items/ItemId.h"
#include "aion/gameserver/model/siege/ArtifactLocation.h"
#include "aion/gameserver/model/siege/FortressLocation.h"
#include "aion/gameserver/model/siege/SiegeModType.h"
#include "aion/gameserver/model/siege/SiegeRaceInfo.h"
#include "aion/gameserver/model/team/legion/Legion.h"
#include "aion/gameserver/model/team/legion/LegionHistoryAction.h"
#include "aion/gameserver/model/team/legion/LegionMember.h"
#include "aion/gameserver/model/team/legion/LegionWarehouse.h"
#include "aion/gameserver/model/templates/npc/AbyssNpcType.h"
#include "aion/gameserver/model/templates/npc/NpcRating.h"
#include "aion/gameserver/model/templates/siegelocation/SiegeLegionReward.h"
#include "aion/gameserver/model/templates/siegelocation/SiegeLocationTemplate.h"
#include "aion/gameserver/model/templates/siegelocation/SiegeMercenaryZone.h"
#include "aion/gameserver/model/templates/spawns/siegespawns/SiegeSpawnTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/questEngine/QuestEngine.h"
#include "aion/gameserver/questEngine/model/QuestEnv.h"
#include "aion/gameserver/services/LegionService.h"
#include "aion/gameserver/services/SiegeService.h"
#include "aion/gameserver/services/abyss/GloryPointsService.h"
#include "aion/gameserver/services/mail/AbyssSiegeLevelInfo.h"
#include "aion/gameserver/services/mail/MailFormatter.h"
#include "aion/gameserver/services/mail/SiegeResult.h"
#include "aion/gameserver/services/player/PlayerService.h"
#include "aion/gameserver/services/siege/BalaurAssaultService.h"
#include "aion/gameserver/services/siege/MercenaryLocation.h"
#include "aion/gameserver/services/siege/SiegeCounter.h"
#include "aion/gameserver/services/siege/SiegeRaceCounter.h"
#include "aion/gameserver/skillengine/SkillEngine.h"
#include "aion/gameserver/skillengine/model/Effect.h"
#include "aion/gameserver/skillengine/model/SkillTemplate.h"
#include "aion/gameserver/spawnengine/SpawnEngine.h"
#include "aion/gameserver/utils/ChatUtil.h"
#include "aion/gameserver/utils/JavaMath.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"
#include "aion/gameserver/world/World.h"

namespace aion::gameserver::services::siege {

using model::siege::SiegeModType;
using model::siege::SiegeRace;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;

static const auto log = commons::logging::LoggerFactory::getLogger("SIEGE_LOG");

namespace {
/** Java SiegeRace.getL10n() (the L10n default method): the localized name of the race */
std::string l10nOf(SiegeRace race) {
	return utils::ChatUtil::l10n(model::siege::getL10nId(race));
}
} // namespace

FortressSiege::FortressSiege(model::siege::FortressLocation& fortress) : Siege(fortress) {
}

runtime::Ref<FortressSiege> FortressSiege::create(model::siege::FortressLocation& fortress) {
	return runtime::makeRef<FortressSiege>(fortress);
}

runtime::Ptr<model::siege::FortressLocation> FortressSiege::getSiegeLocation() const {
	return runtime::cast<model::siege::FortressLocation>(Siege::getSiegeLocation());
}

// Java FortressSiege.java onSiegeStart
void FortressSiege::onSiegeStart() {
	if (configs::main::LoggingConfig::LOG_SIEGE.load())
		log.info(toString() + ": Siege started. Race: " + std::string(xml::enumName(getSiegeLocation()->getRace())) + ", legion ID: " +
			std::to_string(getSiegeLocation()->getLegionId()));
	// Mark fortress as vulnerable
	getSiegeLocation()->setVulnerable(true);

	// Let the world know where the siege is
	broadcastState(*getSiegeLocation());

	// Clear fortress from enemys
	getSiegeLocation()->clearLocation();

	// Remove all and spawn siege NPCs
	despawnNpcs(getSiegeLocationId());
	spawnNpcs(getSiegeLocationId(), getSiegeLocation()->getRace(), SiegeModType::SIEGE);
	initSiegeBoss();
	this->oldLegionId.set(getSiegeLocation()->getLegionId());
	if (getSiegeLocation()->getRace() != SiegeRace::BALAUR) {
		initMercenaryZones();
		getSiegeLocation()->forEachPlayer([this](model::gameobjects::player::Player& p) {
			getSiegeLocation()->checkForBalanceBuff(p, model::siege::FortressLocation::SiegeBuffAction::ADD);
		});
		if (getBoss()->getLevel() == 65) {
			const SiegeRace oppositeRace = getSiegeLocation()->getRace() == SiegeRace::ELYOS ? SiegeRace::ASMODIANS : SiegeRace::ELYOS;
			// Faction Balance NPCs (fieldmap FortressSiege@L87:46: a task pinning this)
			utils::ThreadPoolManager::getInstance().schedule(this, [this, oppositeRace] { spawnFactionTroopAssault(oppositeRace); },
				static_cast<int64_t>(commons::utils::Rnd::get(600, 1800)) * 1000);
		}
	}
	// Check for Balaur Assault
	if (configs::main::SiegeConfig::BALAUR_AUTO_ASSAULT.load())
		BalaurAssaultService::getInstance().onSiegeStart(*this);
}

// Java FortressSiege.java spawnFactionTroopAssault
void FortressSiege::spawnFactionTroopAssault(SiegeRace race) {
	if (!getSiegeLocation()->isVulnerable())
		return;

	const int32_t worldId = getSiegeLocation()->getWorldId();
	for (const runtime::Ptr<model::gameobjects::siege::SiegeNpc>& sn : world::World::getInstance().getLocalSiegeNpcs(getSiegeLocationId())) {
		if (sn->getAbyssNpcType() == model::templates::npc::AbyssNpcType::ARTIFACT || sn->getRating() == model::templates::npc::NpcRating::LEGENDARY ||
			sn->getSpawn()->getSiegeModType() == SiegeModType::ASSAULT || commons::utils::Rnd::chance() < 35)
			continue;
		const int32_t amount = commons::utils::Rnd::get(1, 2);
		for (int32_t i = 0; i < amount; i++) {
			const double angleRadians = static_cast<double>(commons::utils::Rnd::nextFloat(180.0f)) * 0.017453292519943295; // Math.toRadians
			const float x1 = static_cast<float>(sn->getX() + std::cos(angleRadians) * commons::utils::Rnd::get(1, 2));
			const float y1 = static_cast<float>(sn->getY() + std::sin(angleRadians) * commons::utils::Rnd::get(1, 2));
			runtime::Ref<model::templates::spawns::siegespawns::SiegeSpawnTemplate> temp = spawnengine::SpawnEngine::newSiegeSpawn(worldId,
				race == SiegeRace::ELYOS ? commons::utils::Rnd::get(252408, 252412) : commons::utils::Rnd::get(252413, 252417), getSiegeLocationId(), race,
				SiegeModType::ASSAULT, x1, y1, sn->getZ(), int8_t{0});
			spawnengine::SpawnEngine::spawnObject(*temp, 1);
		}
	}
}

// Java FortressSiege.java initMercenaryZones
void FortressSiege::initMercenaryZones() {
	// can be null if not implemented (Java); C++: an empty list
	for (const model::templates::siegelocation::SiegeMercenaryZone* zone : getSiegeLocation()->getSiegeMercenaryZones()) {
		runtime::Ref<MercenaryLocation> mLoc = MercenaryLocation::create(zone, getSiegeLocation()->getRace(), getSiegeLocationId());
		activeMercenaryLocs.put(zone->getId(), mLoc);
	}
}

// Java FortressSiege.java onSiegeFinish
void FortressSiege::onSiegeFinish() {
	if (configs::main::LoggingConfig::LOG_SIEGE.load()) {
		const SiegeRace oldRace = getSiegeLocation()->getRace();
		const int32_t legionIdBefore = getSiegeLocation()->getLegionId();
		if (isBossKilled()) {
			runtime::Ptr<SiegeRaceCounter> winner = getWinnerRaceCounter();
			const std::optional<int32_t> winnerLegionId = winner->getWinnerLegionId();
			log.info(toString() + ": Siege finished. Old race: " + std::string(xml::enumName(oldRace)) + ", legion ID: " + std::to_string(legionIdBefore) +
				" -> New race: " + std::string(xml::enumName(winner->getSiegeRace())) + ", legion ID: " + std::to_string(winnerLegionId.value_or(0)));
		} else {
			log.info(toString() + ": Siege finished. No winner found. Race: " + std::string(xml::enumName(oldRace)) + ", legion ID: " +
				std::to_string(legionIdBefore));
		}
	}

	// despawn protectors and make fortress invulnerable
	SiegeService::getInstance().deSpawnNpcs(getSiegeLocationId());
	// need to remove balance buff before vulnerability is set to false
	getSiegeLocation()->forEachPlayer([this](model::gameobjects::player::Player& p) {
		getSiegeLocation()->checkForBalanceBuff(p, model::siege::FortressLocation::SiegeBuffAction::SIEGE_END_REMOVE);
	});
	getSiegeLocation()->setVulnerable(false);
	getSiegeLocation()->setUnderShield(false);

	// Guardian deity general was not killed, fortress stays with previous
	if (isBossKilled()) {
		onCapture();
		broadcastUpdate(*getSiegeLocation());
	} else {
		onDefended();
		broadcastState(*getSiegeLocation());
	}
	getSiegeLocation()->adjustFactionBalance(getFactionBalanceAdjustment());

	SiegeService::getInstance().spawnNpcs(getSiegeLocationId(), getSiegeLocation()->getRace(), SiegeModType::PEACE);

	// Reward players and owning legion
	const SiegeRace winnerRace = getSiegeLocation()->getRace();
	if (winnerRace != SiegeRace::BALAUR) {
		const SiegeRace loserRace = winnerRace == SiegeRace::ASMODIANS ? SiegeRace::ELYOS : SiegeRace::ASMODIANS;
		runtime::Ptr<SiegeRaceCounter> winnerRaceCounter = getSiegeCounter()->getRaceCounter(winnerRace);
		runtime::Ptr<SiegeRaceCounter> loserRaceCounter = getSiegeCounter()->getRaceCounter(loserRace);
		sendRewardsToParticipants(*winnerRaceCounter, isBossKilled() ? mail::SiegeResult::OCCUPY : mail::SiegeResult::DEFENDER);
		sendRewardsToParticipants(*loserRaceCounter, isBossKilled() ? mail::SiegeResult::FAIL : mail::SiegeResult::EMPTY);
		distributeLegionRewards(*winnerRaceCounter);
	} else if (configs::main::SiegeConfig::SIEGE_REWARD_BALAUR_VICTORY.load()) {
		sendRewardsToParticipants(*getSiegeCounter()->getRaceCounter(SiegeRace::ASMODIANS), isBossKilled() ? mail::SiegeResult::FAIL : mail::SiegeResult::EMPTY);
		sendRewardsToParticipants(*getSiegeCounter()->getRaceCounter(SiegeRace::ELYOS), isBossKilled() ? mail::SiegeResult::FAIL : mail::SiegeResult::EMPTY);
	}

	// Update outpost status
	// Certain fortresses are changing outpost ownership
	updateOutpostStatusByFortress(*getSiegeLocation());

	// Update data in the DB
	dao::SiegeDAO::updateSiegeLocation(*getSiegeLocation());
	if (isBossKilled()) {
		getSiegeLocation()->forEachPlayer([this](model::gameobjects::player::Player& p) {
			if (model::siege::getByRace(p.getRace()) == getSiegeLocation()->getRace()) {
				runtime::Ref<questEngine::model::QuestEnv> env =
					questEngine::model::QuestEnv::create(runtime::Ptr<model::gameobjects::VisibleObject>(getBoss()), p, 0);
				questEngine::QuestEngine::getInstance().onKill(*env);
			}
		});
	}
}

// Java FortressSiege.java onCapture
void FortressSiege::onCapture() {
	runtime::Ptr<SiegeRaceCounter> winner = getWinnerRaceCounter();
	const SiegeRace winnerRace = winner->getSiegeRace();
	const SiegeRace oldRace = getSiegeLocation()->getRace();
	runtime::Ptr<model::team::legion::Legion> oldLegion = oldLegionId.get() == 0 ? nullptr : LegionService::getInstance().getLegion(oldLegionId.get());

	// Players gain buffs on capture of some fortresses
	applyWorldBuffs(winnerRace, false);
	// Set new fortress and artifact owner race
	getSiegeLocation()->setRace(winnerRace);
	getArtifact()->setRace(winnerRace);

	// reset occupy count
	getSiegeLocation()->setOccupiedCount(winnerRace == SiegeRace::BALAUR ? 0 : 1);

	// If new race is balaur
	if (SiegeRace::BALAUR == winnerRace) {
		getSiegeLocation()->setLegionId(0);
		getArtifact()->setLegionId(0);
	} else {
		const std::optional<int32_t> topLegionId = winner->getWinnerLegionId();
		getSiegeLocation()->setLegionId(topLegionId.value_or(0));
		getArtifact()->setLegionId(topLegionId.value_or(0));
	}

	// announce
	const std::string locL10n = getSiegeLocation()->getTemplate()->getL10n();
	runtime::Ptr<model::team::legion::Legion> winnerLegion =
		getSiegeLocation()->getLegionId() == 0 ? nullptr : LegionService::getInstance().getLegion(getSiegeLocation()->getLegionId());
	SM_SYSTEM_MESSAGE loserMsg = getLoserMsg(oldRace, oldLegion, locL10n);
	SM_SYSTEM_MESSAGE winnerMsg = getWinnerMsg(winnerRace, winnerLegion, locL10n);
	world::World::getInstance().forEachPlayer([&](model::gameobjects::player::Player& player) {
		if (model::getRaceId(player.getRace()) == model::siege::getRaceId(oldRace))
			utils::PacketSendUtility::sendPacket(player, loserMsg);
		else
			utils::PacketSendUtility::sendPacket(player, winnerMsg);
	});
}

// Java FortressSiege.java getWinnerMsg
SM_SYSTEM_MESSAGE FortressSiege::getWinnerMsg(SiegeRace winnerRace, runtime::Ptr<model::team::legion::Legion> winnerLegion, const std::string& locationName) {
	if (!winnerLegion)
		return SM_SYSTEM_MESSAGE::STR_ABYSS_WIN_CASTLE(l10nOf(winnerRace), locationName);
	else
		return SM_SYSTEM_MESSAGE::STR_ABYSS_GUILD_WIN_CASTLE(winnerLegion->getName(), locationName);
}

// Java FortressSiege.java getLoserMsg
SM_SYSTEM_MESSAGE FortressSiege::getLoserMsg(SiegeRace loserRace, runtime::Ptr<model::team::legion::Legion> oldLegion, const std::string& locationName) {
	if (!oldLegion)
		return SM_SYSTEM_MESSAGE::STR_ABYSS_CASTLE_TAKEN(l10nOf(loserRace), locationName);
	else
		return SM_SYSTEM_MESSAGE::STR_ABYSS_GUILD_CASTLE_TAKEN(oldLegion->getName(), locationName);
}

// Java FortressSiege.java getFactionBalanceAdjustment
int32_t FortressSiege::getFactionBalanceAdjustment() {
	switch (getSiegeLocation()->getRace()) {
		case SiegeRace::ELYOS:
			return 1;
		case SiegeRace::ASMODIANS:
			return -1;
		case SiegeRace::BALAUR: {
			const int32_t b = getSiegeLocation()->getFactionBalance();
			if (b > 0)
				return -1;
			else if (b < 0)
				return 1;
			break;
		}
	}
	return 0;
}

// Java FortressSiege.java onDefended
void FortressSiege::onDefended() {
	// Increase fortress occupied count
	if (getSiegeLocation()->getRace() != SiegeRace::BALAUR && getSiegeLocation()->getTemplate()->getMaxOccupyCount() > 0) {
		getSiegeLocation()->increaseOccupiedCount();
	}

	// Players gain buffs for successfully defense / failed capture the fortress
	applyWorldBuffs(getSiegeLocation()->getRace(), true);
}

// Java FortressSiege.java applyWorldBuffs
void FortressSiege::applyWorldBuffs(SiegeRace winner, bool isDefense) {
	int32_t skillId;

	switch (getSiegeLocation()->getLocationId()) {
		case 1131:
			skillId = 12147;
			break;
		case 1132:
			skillId = 12148;
			break;
		case 1141:
			skillId = 12149;
			break;
		case 1221:
			skillId = 12075;
			break;
		case 1231:
			skillId = 12076;
			break;
		case 1241:
			skillId = 12077;
			break;
		case 1251:
			skillId = 12074;
			break;
		case 2011:
			skillId = 12155;
			break;
		case 2021:
			skillId = 12156;
			break;
		case 3011:
			skillId = 12157;
			break;
		case 3021:
			skillId = 12158;
			break;
		default:
			return;
	}

	const skillengine::model::SkillTemplate* skillTemplate = dataholders::DataManager::SKILL_DATA->getSkillTemplate(skillId);
	if (skillTemplate == nullptr)
		throw runtime::NullPointerException("no skill template " + std::to_string(skillId)); // Java: getSkillTemplate(skillId).getL10n()
	const std::string skillL10n = utils::ChatUtil::l10n(skillTemplate->getL10nId());
	SM_SYSTEM_MESSAGE notification = isDefense
		? SM_SYSTEM_MESSAGE::STR_CASTLE_DEFENCE_WIN_BUFF_ON(l10nOf(winner), getSiegeLocation()->getTemplate()->getL10n(), skillL10n)
		: SM_SYSTEM_MESSAGE::STR_CASTLE_WIN_BUFF_ON(l10nOf(winner), getSiegeLocation()->getTemplate()->getL10n(), skillL10n);
	world::World::getInstance().forEachPlayer([&](model::gameobjects::player::Player& player) {
		if (model::getRaceId(player.getRace()) == model::siege::getRaceId(winner)) {
			skillengine::SkillEngine::getInstance().applyEffectDirectly(skillId, player, player);
			utils::PacketSendUtility::sendPacket(player, notification);
		}
	});
}

// Java FortressSiege.java distributeLegionRewards(SiegeRaceCounter)
void FortressSiege::distributeLegionRewards(SiegeRaceCounter& winnerRaceCounter) {
	const int32_t legionId = getSiegeLocation()->getLegionId();
	runtime::Ptr<model::team::legion::Legion> legion = legionId == 0 ? nullptr : LegionService::getInstance().getLegion(legionId);
	if (!legion) {
		if (configs::main::LoggingConfig::LOG_SIEGE.load())
			log.info(toString() + ": Skipped sending legion rewards because the fortress is not owned by any legion (owner race: " +
				std::string(xml::enumName(getSiegeLocation()->getRace())) + ").");
		return;
	}
	distributeLegionGp(*legion, winnerRaceCounter);
	distributeLegionRewards(*legion);
}

// Java FortressSiege.java distributeLegionGp
void FortressSiege::distributeLegionGp(model::team::legion::Legion& legion, SiegeRaceCounter& src) {
	const int32_t legionGp = getSiegeLocation()->getLegionGp();
	if (legionGp <= 0)
		return;
	try {
		// Java: a HashSet of the AP makers' ids, retainAll(legion.getMemberIds()); C++: ordered by id. The order shows only in the log line (Java's
		// HashSet iterates by bucket); each member gets the same GP whatever the order
		std::set<int32_t> participatedLegionMembers;
		const std::vector<int32_t> memberIds = legion.getMemberIds()->snapshot();
		for (const auto& [playerId, points] : src.getPlayerAbyssPoints())
			if (std::find(memberIds.begin(), memberIds.end(), playerId) != memberIds.end())
				participatedLegionMembers.insert(playerId);

		if (participatedLegionMembers.empty()) {
			if (configs::main::LoggingConfig::LOG_SIEGE.load())
				log.info(toString() + ": Distributed no GP to the members of " + legion.toString() + " because no one made AP");
		} else {
			const int32_t gp = std::min(utils::JavaMath::round(static_cast<float>(legionGp) / static_cast<float>(participatedLegionMembers.size())),
				configs::main::SiegeConfig::LEGION_GP_CAP_PER_MEMBER.load());
			for (int32_t participant : participatedLegionMembers)
				abyss::GloryPointsService::addGp(participant, gp);
			if (configs::main::LoggingConfig::LOG_SIEGE.load()) {
				std::string ids = "[";
				bool first = true;
				for (int32_t participant : participatedLegionMembers) {
					ids += (first ? "" : ", ") + std::to_string(participant);
					first = false;
				}
				log.info(toString() + ": Distributed " + std::to_string(gp) + " GP each, to the following members of " + legion.toString() + ": " + ids + "]");
			}
		}
	} catch (const std::exception& e) {
		log.error("Error while distributing legion GP for " + toString(), e);
	}
}

// Java FortressSiege.java distributeLegionRewards(Legion)
void FortressSiege::distributeLegionRewards(model::team::legion::Legion& legion) {
	const std::vector<const model::templates::siegelocation::SiegeLegionReward*> legionRewards = getSiegeLocation()->getLegionRewards();
	if (legionRewards.empty()) // Java: legionRewards == null || legionRewards.isEmpty()
		return;
	try {
		int64_t totalKinah = 0;
		int32_t nonKinahItems = 0;
		runtime::Ptr<model::team::legion::LegionMember> general = legion.getBrigadeGeneral();
		if (!general)
			throw runtime::NullPointerException("legion without brigade general"); // Java: legion.getBrigadeGeneral().getObjectId()
		runtime::Ref<model::gameobjects::player::PlayerCommonData> brigadeGeneral = player::PlayerService::getOrLoadPlayerCommonData(general->getObjectId());
		for (const model::templates::siegelocation::SiegeLegionReward* item : legionRewards) {
			if (item->getItemId() == model::items::ItemId::KINAH) {
				const int64_t kinah = isBossKilled() ? item->getItemCount() : utils::JavaMath::round(static_cast<float>(item->getItemCount()) * 0.7f);
				legion.getLegionWarehouse().increaseKinah(kinah);
				LegionService::getInstance().addRewardHistory(legion, kinah,
					isBossKilled() ? model::team::legion::LegionHistoryAction::OCCUPATION : model::team::legion::LegionHistoryAction::DEFENSE,
					getSiegeLocationId());
				totalKinah += kinah;
			} else {
				nonKinahItems++;
				if (!brigadeGeneral)
					throw runtime::NullPointerException("PlayerCommonData of the brigade general"); // Java: NPE inside the formatter
				mail::MailFormatter::sendAbyssRewardMail(*getSiegeLocation(), *brigadeGeneral, mail::AbyssSiegeLevel::NONE, mail::SiegeResult::PROTECT,
					commons::utils::currentTimeMillis(), item->getItemId(), item->getItemCount(), 0);
			}
		}
		if (configs::main::LoggingConfig::LOG_SIEGE.load()) {
			std::string msg;
			if (totalKinah > 0)
				msg += "Added " + std::to_string(totalKinah) + " kinah to the legion warehouse";
			if (nonKinahItems > 0)
				msg += std::string(msg.empty() ? "Sent " : " and sent ") + std::to_string(nonKinahItems) + " legion rewards to brigade general " +
					brigadeGeneral->getName() + " of " + legion.toString() + " (see sysmail.log)";
			log.info(toString() + ": " + msg);
		}
	} catch (const std::exception& e) {
		log.error("Error while distributing legion rewards for " + toString(), e);
	}
}

// Java FortressSiege.java isEndless
bool FortressSiege::isEndless() {
	return false;
}

// Java FortressSiege.java onAbyssPointsAdded
void FortressSiege::onAbyssPointsAdded(model::gameobjects::player::Player& player, int32_t abyssPoints) {
	if (getSiegeLocation()->isVulnerable() && getSiegeLocation()->isInsideLocation(player))
		getSiegeCounter()->addAbyssPoints(player, abyssPoints);
}

// Java FortressSiege.java getArtifact
runtime::Ptr<model::siege::ArtifactLocation> FortressSiege::getArtifact() {
	return SiegeService::getInstance().getFortressArtifact(getSiegeLocationId());
}

// Java FortressSiege.java hasArtifact
bool FortressSiege::hasArtifact() {
	return static_cast<bool>(getArtifact());
}

// Java FortressSiege.java getMercenaryLocationByZoneId
runtime::Ptr<MercenaryLocation> FortressSiege::getMercenaryLocationByZoneId(int32_t zoneId) {
	return activeMercenaryLocs.get(zoneId);
}

FortressSiege::~FortressSiege() = default;

} // namespace aion::gameserver::services::siege
