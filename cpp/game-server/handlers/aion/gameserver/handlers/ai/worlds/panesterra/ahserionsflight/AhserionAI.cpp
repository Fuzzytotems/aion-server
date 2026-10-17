#include "aion/gameserver/handlers/ai/worlds/panesterra/ahserionsflight/AhserionAI.h"

#include <algorithm>
#include <string>
#include <vector>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/configs/main/SiegeConfig.h"
#include "aion/gameserver/controllers/attack/AggroList.h"
#include "aion/gameserver/controllers/attack/DamageInfo.h"
#include "aion/gameserver/controllers/attack/DamageList.h"
#include "aion/gameserver/model/stats/calc/Stat2.h"
#include "aion/gameserver/model/stats/container/NpcGameStats.h"
#include "aion/gameserver/model/templates/world/WorldMapTemplate.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/services/panesterra/PanesterraService.h"
#include "aion/gameserver/services/panesterra/ahserion/AhserionRaid.h"
#include "aion/gameserver/services/panesterra/ahserion/PanesterraTeam.h"
#include "aion/gameserver/skillengine/SkillEngine.h"
#include "aion/gameserver/skillengine/model/Effect.h"
#include "aion/gameserver/skillengine/model/Skill.h"
#include "aion/gameserver/skillengine/model/SkillTemplate.h"
#include "aion/gameserver/utils/PositionUtil.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"
#include "aion/gameserver/world/knownlist/KnownList.h"

namespace aion::gameserver::handlers::ai::worlds::panesterra::ahserionsflight {

AION_AI(AhserionAI, "ahserion");

using services::panesterra::ahserion::PanesterraFaction;

const commons::logging::Logger AhserionAI::log = commons::logging::LoggerFactory::getLogger("SIEGE_LOG");

// Java AhserionAI.java:43-47
void AhserionAI::modifyOwnerStat(Stat2& stat) {
	if (stat.getStat() == StatEnum::MAXHP)
		stat.setBaseRate(SiegeConfig::AHSERION_MAX_PLAYERS_PER_TEAM.load() / 100.0f);
}

// Java AhserionAI.java:49-54
float AhserionAI::modifyDamage(Creature& attacker, float damage, runtime::Ptr<Effect> effect) {
	if (effect != nullptr && effect->getSkillId() == 21583) // Artillery Blast
		return damage * (SiegeConfig::AHSERION_MAX_PLAYERS_PER_TEAM.load() / 100.0f);
	return AggressiveNpcAI::modifyDamage(attacker, damage, effect);
}

// Java AhserionAI.java:56-79 (no super call, as Java)
void AhserionAI::onStartUseSkill(const skillengine::model::SkillTemplate* st, int32_t lv) {
	if (st == nullptr) // Java: st.getSkillId() on null
		throw runtime::NullPointerException("AhserionAI: skill template");
	switch (st->getSkillId()) {
		case 21566: // Thunder Crash
			if (lv == 55 && commons::utils::currentTimeMillis() - getOwner().getGameStats()->getFightStartingTime() < 30000)
				PacketSendUtility::broadcastMessage(runtime::Ptr<Npc>(getOwner()), 1501157); // _01: Beritra thinks me unready. I shall prove myself in your slaughter!
			break;
		case 21570: // Death Shriek
			if (lv == 58)
				PacketSendUtility::broadcastMessage(runtime::Ptr<Npc>(getOwner()), 1501163); // _07: Not enough time... My Lords, come save me!
			break;
		case 21571: { // Ereshkigal's Reign
			runtime::Ptr<world::WorldPosition> p = getPosition();
			spawn(297186, p->getX(), p->getY(), p->getZ() + 10, int8_t{0}); // Ereshkigal's Voice
			break;
		}
		case 21573: // Distortion Pulse
			if (lv == 58)
				PacketSendUtility::broadcastMessage(runtime::Ptr<Npc>(getOwner()), 1501162); // _06: Impudent pestilence! Be gone!
			break;
		// Ide Destruction
		case 21574:
			PacketSendUtility::broadcastMessage(runtime::Ptr<Npc>(getOwner()), 1501164); // _08: I will return you to your beloved Aion's dust!
			break;
		default:
			break;
	}
}

// Java AhserionAI.java:81-97 (no super call, as Java; AggroList.addHate of a null target does nothing: isAware(null) is false)
void AhserionAI::onEndUseSkill(const skillengine::model::SkillTemplate* st, int32_t lv) {
	if (st == nullptr) // Java: st.getSkillId() on null
		throw runtime::NullPointerException("AhserionAI: skill template");
	switch (st->getSkillId()) {
		case 21564: // Dragon Claw
			if (lv == 58)
				handleBaseAssault();
			break;
		case 21567: // Toxic Spew
			if (lv == 58)
				handleSupportSpawns();
			break;
		case 21574:
			handleIdeDestruction(); // Ide Destruction
			break;
		default:
			break;
	}
	// Custom solution to resolve the retail add hate event (switch_target_by_attacker_indicator)
	if (lv == 57 || lv == 26) {
		runtime::Ptr<Creature> target = getAggroList().getTarget(AggroTarget::RANDOM);
		if (target != nullptr)
			getAggroList().addHate(*target, 100000);
	}
}

// Java AhserionAI.java:99-103
void AhserionAI::onEffectApplied(Effect& effect) {
	if (effect.getSkillId() == 21571) // Ereshkigal's Reign
		PacketSendUtility::broadcastMessage(runtime::Ptr<Npc>(getOwner()), 1501159); // _03: My head... Nooooo! Get out of my head!
}

// Java AhserionAI.java:105-109
void AhserionAI::onEffectEnd(runtime::Ptr<Effect> effect) {
	if (effect->getSkillId() == 21571) // Ereshkigal's Reign
		PacketSendUtility::broadcastMessage(runtime::Ptr<Npc>(getOwner()), 1501161); // _03: Ereshkigal...? No! Nooooooo-argh!
}

// Java AhserionAI.java:111-116
void AhserionAI::handleSupportSpawns() {
	spawn(297353, 475.530f, 536.809f, 675.989f, int8_t{75});
	spawn(297353, 532.324f, 545.949f, 675.746f, int8_t{75});
	spawn(297353, 541.742f, 491.245f, 675.462f, int8_t{75});
	spawn(297353, 487.297f, 479.381f, 678.300f, int8_t{75});
}

// Java AhserionAI.java:118-126
void AhserionAI::handleBaseAssault() {
	if (getOwner().getWorldId() == 400030000 && AhserionRaid::getInstance().isStarted()) {
		for (uint8_t value = 0; value <= static_cast<uint8_t>(PanesterraFaction::PEACE); value++) { // Java: PanesterraFaction.values()
			const PanesterraFaction faction = static_cast<PanesterraFaction>(value);
			runtime::Ptr<services::panesterra::ahserion::PanesterraTeam> team = PanesterraService::getInstance().getTeam(faction);
			if (team != nullptr && !team->isEliminated())
				AhserionRaid::getInstance().spawnStage(5, faction);
		}
	}
}

// Java AhserionAI.java:128-134
void AhserionAI::handleIdeDestruction() {
	for (const runtime::Ptr<Player>& p : getKnownList().streamPlayers()) {
		if (!p->isDead() && PositionUtil::isInRange(*p, 509.64f, 513.25f, 675.145f, 45)) {
			runtime::Ref<skillengine::model::Skill> skill = SkillEngine::getInstance().getSkill(getOwner(), 21575, 1, runtime::Ptr<VisibleObject>(p));
			if (skill == nullptr) // Java: getSkill(...).useWithoutPropSkill() on null
				throw runtime::NullPointerException("AhserionAI: skill 21575");
			skill->useWithoutPropSkill();
		}
	}
}

// Java AhserionAI.java:136-155
void AhserionAI::handleDied() {
	if (getOwner().getWorldId() == 400030000 && AhserionRaid::getInstance().isStarted()) {
		std::map<PanesterraFaction, int32_t> panesterraDamage;
		// Only players can attack Ahserion on this map.
		for (const controllers::attack::DamageInfo& damageInfo : getAggroList().getFinalDamageList().getCreatureDamages()) {
			if (runtime::Ptr<Player> player = runtime::as<Player>(damageInfo.getAttacker())) {
				runtime::Ptr<services::panesterra::ahserion::PanesterraTeam> team = PanesterraService::getInstance().getTeam(*player);
				if (team != nullptr && !team->isEliminated()) {
					PanesterraFaction faction = team->getFaction();
					panesterraDamage[faction] += damageInfo.getDamage(); // Java: merge(faction, damage, Integer::sum)
				}
			}
		}
		std::optional<PanesterraFaction> winner = findWinnerTeam(panesterraDamage);
		if (winner)
			AhserionRaid::getInstance().handleBossKilled(getOwner(), *winner);
	}
	logMetrics();
	AggressiveNpcAI::handleDied();
}

// Java AhserionAI.java:157-170 (getTeam(faction).isEliminated() on a team gone since the damage was counted is Java's NullPointerException)
std::optional<PanesterraFaction> AhserionAI::findWinnerTeam(const std::map<PanesterraFaction, int32_t>& panesterraDamage) {
	std::optional<PanesterraFaction> winner;
	int32_t maxDmg = 0;
	for (uint8_t value = 0; value <= static_cast<uint8_t>(PanesterraFaction::PEACE); value++) { // Java: PanesterraFaction.values()
		const PanesterraFaction faction = static_cast<PanesterraFaction>(value);
		const auto dmg = panesterraDamage.find(faction);
		if (dmg == panesterraDamage.end())
			continue;
		runtime::Ptr<services::panesterra::ahserion::PanesterraTeam> team = PanesterraService::getInstance().getTeam(faction);
		if (team == nullptr)
			throw runtime::NullPointerException("AhserionAI: no Panesterra team");
		if (!team->isEliminated()) {
			if (dmg->second > maxDmg) {
				maxDmg = dmg->second;
				winner = faction;
			}
		}
	}
	return winner;
}

// Java AhserionAI.java:172-180
void AhserionAI::logMetrics() {
	int64_t fullFightTime = (commons::utils::currentTimeMillis() - getOwner().getGameStats()->getFightStartingTime()) / 1000;
	std::vector<controllers::attack::DamageInfo> damages = getAggroList().getFinalDamageList().getCreatureDamages();
	std::stable_sort(damages.begin(), damages.end(), [](const controllers::attack::DamageInfo& a, const controllers::attack::DamageInfo& b) {
		return a.getDamage() > b.getDamage(); // Java: Comparator.comparingInt(DamageInfo::getDamage).reversed()
	});
	std::string damageDealt;
	for (const controllers::attack::DamageInfo& ai : damages) {
		if (!damageDealt.empty())
			damageDealt += ", ";
		damageDealt += ai.getAttacker()->getName() + " (ID: " + std::to_string(ai.getAttacker()->getObjectId()) + ", Dmg: " +
			std::to_string(ai.getDamage()) + ")";
	}
	log.info("[" + std::string(getPosition()->getWorldMapInstance()->getTemplate()->getName()) + "] " + getOwner().getName() + " (ID:" +
		std::to_string(getNpcId()) + ") was killed in " + std::to_string(fullFightTime) + "s. Damage List: " + damageDealt);
}

} // namespace aion::gameserver::handlers::ai::worlds::panesterra::ahserionsflight
