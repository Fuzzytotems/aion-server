#include "aion/gameserver/model/gameobjects/player/npcFaction/NpcFactions.h"

#include <chrono>
#include <string>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/utils/Rnd.h"
#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/configs/main/GSConfig.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/NpcFactionsData.h"
#include "aion/gameserver/dataholders/QuestsData.h"
#include "aion/gameserver/model/Race.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"
#include "aion/gameserver/model/gameobjects/player/RequestResponseHandler.h"
#include "aion/gameserver/model/gameobjects/player/detail/PlayerMath.h"
#include "aion/gameserver/model/gameobjects/player/ResponseRequester.h"
#include "aion/gameserver/model/gameobjects/player/npcFaction/ENpcFactionQuestState.h"
#include "aion/gameserver/model/gameobjects/player/npcFaction/NpcFaction.h"
#include "aion/gameserver/model/skill/PlayerSkillEntry.h"
#include "aion/gameserver/model/skill/PlayerSkillList.h"
#include "aion/gameserver/model/templates/QuestTemplate.h"
#include "aion/gameserver/model/templates/factions/FactionCategory.h"
#include "aion/gameserver/model/templates/factions/NpcFactionTemplate.h"
#include "aion/gameserver/model/templates/quest/QuestMentorType.h"
#include "aion/gameserver/network/aion/serverpackets/SM_DIALOG_WINDOW.h"
#include "aion/gameserver/network/aion/serverpackets/SM_QUESTION_WINDOW.h"
#include "aion/gameserver/network/aion/serverpackets/SM_QUEST_ACTION.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_TITLE_INFO.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/services/QuestService.h"
#include "aion/gameserver/utils/PacketSendUtility.h"

namespace aion::gameserver::model::gameobjects::player::npcFaction {

namespace {

using network::aion::serverpackets::SM_DIALOG_WINDOW;
using network::aion::serverpackets::SM_QUESTION_WINDOW;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;

const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.model.gameobjects.player.npcFaction.NpcFactions");

/** Java dereferences the template of a faction without a null check: NullPointerException for an unknown faction */
const templates::factions::NpcFactionTemplate& templateOrThrow(const templates::factions::NpcFactionTemplate* npcFactionTemplate) {
	if (npcFactionTemplate == nullptr)
		throw runtime::NullPointerException("NpcFactionTemplate is null");
	return *npcFactionTemplate;
}

} // namespace

NpcFactions::NpcFactions(Player& ownerValue)
	: OwnedPart(ownerValue), owner(ownerValue), activeNpcFaction(runtime::Array<runtime::Ref<NpcFaction>>::make(2)),
	  timeLimit(runtime::Array<int32_t>::of({0, 0})) {
}

NpcFactions::~NpcFactions() = default;

void NpcFactions::addNpcFaction(NpcFaction& faction) {
	factions.put(faction.getId(), runtime::Ref<NpcFaction>(faction));
	int32_t type = 0;
	if (faction.isMentor())
		type = 1;

	if (faction.isActive())
		(*activeNpcFaction)[type].set(runtime::Ptr<NpcFaction>(faction));
	if (faction.getTime() == -1) {
		// used to reset from quest daily command
		faction.setTime(static_cast<int32_t>(commons::utils::currentTimeMillis() / 1000));
		(*timeLimit)[type].set(faction.getTime());
	} else if ((*timeLimit)[type].get() < faction.getTime() && faction.getState() == ENpcFactionQuestState::COMPLETE) {
		(*timeLimit)[type].set(faction.getTime());
	}
}

runtime::Ptr<NpcFaction> NpcFactions::getFactionById(int32_t id) {
	return factions.get(id);
}

std::vector<runtime::Ptr<NpcFaction>> NpcFactions::getNpcFactions() {
	return factions.values();
}

runtime::Ptr<NpcFaction> NpcFactions::getActiveNpcFaction(bool mentor) {
	return (*activeNpcFaction)[mentor ? 1 : 0].get();
}

runtime::Ptr<NpcFaction> NpcFactions::setActive(int32_t npcFactionId) {
	runtime::Ptr<NpcFaction> npcFaction = factions.computeIfAbsent(
		npcFactionId, [](int32_t factionId) { return NpcFaction::create(factionId, 0, false, ENpcFactionQuestState::NOTING, 0); });
	npcFaction->setActive(true);
	(*activeNpcFaction)[npcFaction->isMentor() ? 1 : 0].set(npcFaction);
	return npcFaction;
}

void NpcFactions::leaveNpcFaction(Npc& npc) {
	int32_t targetObjectId = npc.getObjectId();
	const templates::factions::NpcFactionTemplate* npcFactionTemplate = dataholders::DataManager::NPC_FACTIONS_DATA->getNpcFactionByNpcId(npc.getNpcId());
	if (npcFactionTemplate == nullptr)
		return;
	runtime::Ptr<NpcFaction> npcFaction = getFactionById(npcFactionTemplate->getId());
	if (!npcFaction || !npcFaction->isActive()) {
		utils::PacketSendUtility::sendPacket(owner, SM_DIALOG_WINDOW(targetObjectId, 1438));
		return;
	}
	utils::PacketSendUtility::sendPacket(owner, SM_DIALOG_WINDOW(targetObjectId, 1353));
	leaveNpcFaction(*npcFaction);
}

void NpcFactions::leaveNpcFaction(NpcFaction& npcFaction) {
	const templates::factions::NpcFactionTemplate& npcFactionTemplate =
		templateOrThrow(dataholders::DataManager::NPC_FACTIONS_DATA->getNpcFactionById(npcFaction.getId()));

	utils::PacketSendUtility::sendPacket(owner, SM_SYSTEM_MESSAGE::STR_FACTION_LEAVE(npcFactionTemplate.getL10n()));
	npcFaction.setActive(false);
	(*activeNpcFaction)[npcFactionTemplate.isMentor() ? 1 : 0].set(runtime::Ref<NpcFaction>());
	if (npcFaction.getState() == ENpcFactionQuestState::START) {
		services::QuestService::abandonQuest(owner, npcFaction.getQuestId());
		npcFaction.setState(ENpcFactionQuestState::NOTING);
	}
}

void NpcFactions::enterGuild(Npc& npc) {
	int32_t targetObjectId = npc.getObjectId();
	const templates::factions::NpcFactionTemplate* npcFactionTemplate = dataholders::DataManager::NPC_FACTIONS_DATA->getNpcFactionByNpcId(npc.getNpcId());
	if (npcFactionTemplate == nullptr) {
		log.warn("Missing faction for faction registrar npc " + std::to_string(npc.getNpcId()));
		return;
	}
	runtime::Ptr<NpcFaction> npcFaction = getFactionById(npcFactionTemplate->getId());
	runtime::Ptr<NpcFaction> activeFaction = getActiveNpcFaction(npcFactionTemplate->isMentor());
	int32_t npcFactionId = npcFactionTemplate->getId();
	int32_t skillPoints = npcFactionTemplate->getSkillPoints();
	Player& player = owner;
	if (skillPoints != 0) {
		bool canEnter = false;
		if (npcFactionTemplate->getCategory() == templates::factions::FactionCategory::COMBINESKILL) {
			for (const runtime::Ptr<skill::PlayerSkillEntry>& skill : player.getSkillList()->getAllSkills()) {
				if (skill->isCraftingSkill() && skill->getSkillLevel() >= skillPoints) {
					canEnter = true;
					break;
				}
			}
		}
		if (!canEnter) {
			utils::PacketSendUtility::sendPacket(player, SM_DIALOG_WINDOW(targetObjectId, 1098));
			return;
		}
	}
	if (player.getLevel() < npcFactionTemplate->getMinLevel() || player.getLevel() > npcFactionTemplate->getMaxLevel()) {
		utils::PacketSendUtility::sendPacket(player, SM_DIALOG_WINDOW(targetObjectId, 1182));
		return;
	}
	// Java: owner.getRace() != template.getRace() && !template.getRace().equals(Race.NPC): a null race is unequal and then throws
	if (player.getRace() != npcFactionTemplate->getRace()) {
		if (!npcFactionTemplate->getRace())
			throw runtime::NullPointerException("NpcFactionTemplate.race is null");
		if (*npcFactionTemplate->getRace() != Race::NPC) {
			utils::PacketSendUtility::sendPacket(player, SM_DIALOG_WINDOW(targetObjectId, 1097));
			return;
		}
	}
	if (npcFaction && npcFaction->isActive()) {
		utils::PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_FACTION_CAN_NOT_JOIN());
		return;
	}
	if (activeFaction && activeFaction->getId() != npcFactionId) {
		askLeaveNpcFaction(npc);
		return;
	}
	if (!npcFaction || !npcFaction->isActive()) {
		utils::PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_FACTION_JOIN(npcFactionTemplate->getL10n()));
		utils::PacketSendUtility::sendPacket(player, SM_DIALOG_WINDOW(targetObjectId, 1012));
		setActive(npcFactionId);
		sendDailyQuest();
	}
}

// fieldmap-class: com.aionemu.gameserver.model.gameobjects.player.npcFaction.NpcFactions$1
/** Java: the anonymous RequestResponseHandler<Player> of askLeaveNpcFaction (fieldmap callback struct, a friend of NpcFactions) */
class NpcFactions_RequestResponseHandler final : public RequestResponseHandler {
	AION_MAKE_REF_FRIEND
public:
	const runtime::Ref<NpcFactions> npcFactions;     // captured this NpcFactions
	const runtime::Ref<NpcFaction> activeNpcFaction; // captured local NpcFaction activeNpcFaction
	const runtime::Ref<Npc> npc;                     // captured param Npc npc

	static runtime::Ref<NpcFactions_RequestResponseHandler> create(Player& player, NpcFactions& npcFactionsValue, NpcFaction& activeNpcFactionValue,
		Npc& npcValue) {
		return runtime::makeRef<NpcFactions_RequestResponseHandler>(player, npcFactionsValue, activeNpcFactionValue, npcValue);
	}

	void acceptRequest(runtime::Ptr<Creature> requesterValue, Player& responder) override {
		static_cast<void>(requesterValue);
		static_cast<void>(responder);
		npcFactions->leaveNpcFaction(*activeNpcFaction);
		npcFactions->enterGuild(*npc);
	}

protected:
	NpcFactions_RequestResponseHandler(Player& player, NpcFactions& npcFactionsValue, NpcFaction& activeNpcFactionValue, Npc& npcValue)
		: RequestResponseHandler(runtime::Ptr<Creature>(player)), npcFactions(npcFactionsValue), activeNpcFaction(activeNpcFactionValue),
		  npc(npcValue) {}
	~NpcFactions_RequestResponseHandler() override = default;
};

void NpcFactions::askLeaveNpcFaction(Npc& npc) {
	const templates::factions::NpcFactionTemplate& npcFactionTemplate =
		templateOrThrow(dataholders::DataManager::NPC_FACTIONS_DATA->getNpcFactionByNpcId(npc.getNpcId()));
	runtime::Ptr<NpcFaction> activeFaction = getActiveNpcFaction(npcFactionTemplate.isMentor());
	if (!activeFaction)
		throw runtime::NullPointerException("activeNpcFaction is null");
	const templates::factions::NpcFactionTemplate* activeNpcFactionTemplate =
		dataholders::DataManager::NPC_FACTIONS_DATA->getNpcFactionById(activeFaction->getId());
	Player& player = owner;
	runtime::Ref<NpcFactions_RequestResponseHandler> responseHandler = NpcFactions_RequestResponseHandler::create(player, *this, *activeFaction, npc);
	bool requested = player.getResponseRequester().putRequest(SM_QUESTION_WINDOW::STR_ASK_JOIN_NEW_FACTION, responseHandler);
	if (requested) {
		utils::PacketSendUtility::sendPacket(player, SM_QUESTION_WINDOW(SM_QUESTION_WINDOW::STR_ASK_JOIN_NEW_FACTION, 0, 0,
														 templateOrThrow(activeNpcFactionTemplate).getL10n(), npcFactionTemplate.getL10n()));
	}
}

void NpcFactions::startQuest(const templates::QuestTemplate* questTemplate) {
	runtime::Ptr<NpcFaction> npcFaction = (*activeNpcFaction)[questTemplate->isMentor() ? 1 : 0].get();
	if (!npcFaction)
		return;
	if (npcFaction->getState() != ENpcFactionQuestState::NOTING && npcFaction->getQuestId() == 0)
		return;
	npcFaction->setState(ENpcFactionQuestState::START);
}

void NpcFactions::abortQuest(const templates::QuestTemplate* questTemplate) {
	runtime::Ptr<NpcFaction> npcFaction = factions.get(questTemplate->getNpcFactionId());
	if (!npcFaction || !npcFaction->isActive())
		return;
	npcFaction->setState(ENpcFactionQuestState::NOTING);
	sendDailyQuest();
}

void NpcFactions::completeQuest(const templates::QuestTemplate* questTemplate) {
	runtime::Ptr<NpcFaction> npcFaction = (*activeNpcFaction)[questTemplate->isMentor() ? 1 : 0].get();
	if (!npcFaction)
		return;
	npcFaction->setTime(getNextTime());
	npcFaction->setState(ENpcFactionQuestState::COMPLETE);
	(*timeLimit)[npcFaction->isMentor() ? 1 : 0].set(npcFaction->getTime());
	if (questTemplate->getMentorType() == templates::quest::QuestMentorType::MENTOR) {
		Player& player = owner;
		player.getCommonData()->setMentorFlagTime(static_cast<int32_t>(commons::utils::currentTimeMillis() / 1000) + 60 * 60 * 24); // TODO 1 day
		utils::PacketSendUtility::broadcastPacket(player, network::aion::serverpackets::SM_TITLE_INFO(player, true), false);
		utils::PacketSendUtility::sendPacket(player, network::aion::serverpackets::SM_TITLE_INFO(true));
	}
}

void NpcFactions::sendDailyQuest() {
	for (int32_t i = 0; i < 2; i++) {
		runtime::Ptr<NpcFaction> faction = (*activeNpcFaction)[i].get();
		if (!faction || !faction->isActive())
			continue;
		if ((*timeLimit)[i].get() > commons::utils::currentTimeMillis() / 1000)
			continue;
		int32_t questId = 0;
		bool skip = false;
		switch (faction->getState()) {
			case ENpcFactionQuestState::COMPLETE:
				if (faction->getTime() > commons::utils::currentTimeMillis() / 1000)
					skip = true;
				break;
			case ENpcFactionQuestState::START:
				skip = true;
				break;
			case ENpcFactionQuestState::NOTING:
				if (faction->getTime() > commons::utils::currentTimeMillis() / 1000)
					questId = faction->getQuestId();
				break;
		}
		if (skip)
			continue;

		if (questId == 0) {
			std::vector<const templates::QuestTemplate*> quests = dataholders::DataManager::QUEST_DATA->getQuestsByNpcFaction(faction->getId(), owner);
			if (quests.empty())
				continue;
			questId = (*commons::utils::Rnd::get(quests))->getId();
			faction->setQuestId(questId);
			faction->setTime(getNextTime());
			faction->setState(ENpcFactionQuestState::NOTING);
		}
		utils::PacketSendUtility::sendPacket(owner, network::aion::serverpackets::SM_QUEST_ACTION(questId));
	}
}

void NpcFactions::onLevelUp() {
	Player& player = owner;
	for (int32_t i = 0; i < 2; i++) {
		runtime::Ptr<NpcFaction> faction = (*activeNpcFaction)[i].get();
		if (!faction || !faction->isActive())
			continue;
		const templates::factions::NpcFactionTemplate& npcFactionTemplate =
			templateOrThrow(dataholders::DataManager::NPC_FACTIONS_DATA->getNpcFactionById(faction->getId()));
		if (npcFactionTemplate.getMaxLevel() < player.getLevel()) {
			faction->setActive(false);
			(*activeNpcFaction)[i].set(runtime::Ref<NpcFaction>());
			if (faction->getState() == ENpcFactionQuestState::START)
				services::QuestService::abandonQuest(player, faction->getQuestId());
			utils::PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_FACTION_LEAVE_BY_LEVEL_LIMIT(npcFactionTemplate.getL10n()));
			faction->setState(ENpcFactionQuestState::NOTING);
		}
	}
}

int32_t NpcFactions::getNextTime() {
	// Java: ServerTime.now(); 9:00 AM tomorrow if it is 9:00 or later, otherwise 9:00 AM today
	const std::chrono::time_zone* zone = configs::main::GSConfig::TIME_ZONE_ID.load();
	if (zone == nullptr)
		zone = std::chrono::current_zone();
	return detail::npcFactionNextTime(commons::utils::currentTimeMillis(), zone);
}

bool NpcFactions::canStartQuest(const templates::QuestTemplate* template_) {
	int32_t type = template_->isMentor() ? 1 : 0;
	return (*activeNpcFaction)[type].get() && (*timeLimit)[type].get() < commons::utils::currentTimeMillis() / 1000;
}

} // namespace aion::gameserver::model::gameobjects::player::npcFaction
