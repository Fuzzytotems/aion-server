#include "aion/gameserver/model/DialogPageInfo.h"

#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"
#include "aion/gameserver/model/gameobjects/player/QuestStateList.h"
#include "aion/gameserver/model/templates/npc/NpcTemplate.h"
#include "aion/gameserver/model/templates/npc/TalkInfo.h"
#include "aion/gameserver/model/templates/quest/QuestNpc.h"
#include "aion/gameserver/questEngine/QuestEngine.h"
#include "aion/gameserver/questEngine/model/QuestState.h"
#include "aion/gameserver/services/DialogService.h"
#include "aion/gameserver/services/QuestService.h"

namespace aion::gameserver::model {

int32_t getStartPageId(gameobjects::Npc& npc, gameobjects::player::Player& player) {
	const templates::npc::TalkInfo* talkInfo = npc.getObjectTemplate()->getTalkInfo();
	if (talkInfo == nullptr || (!talkInfo->isDialogNpc() && !talkInfo->getFuncDialogIds()))
		return 0; // HTML_PAGE_NULL (npc has no conversation or function dialog)
	if (!services::DialogService::isInteractionAllowed(player, npc))
		return 1011; // HTML_PAGE_SELECT1 (npc will say you are not allowed to use their function)
	bool isFunctionalNpc = talkInfo->getFuncDialogIds().has_value();
	if (isFunctionalNpc || hasQuestInteraction(player, npc))
		return 10; // HTML_PAGE_SELECT_QUEST (npc will offer their function or quests)
	if (player.getCommonData()->isDaeva() && hasAlternativeDialogAfterAscension(npc))
		return 1352; // HTML_PAGE_SELECT2
	return 1011; // HTML_PAGE_SELECT1 (default dialog)
}

bool hasAlternativeDialogAfterAscension(gameobjects::Npc& npc) {
	switch (npc.getNpcId()) {
		// Poeta
		case 203049: // elpis
		case 203050: // kales
		case 203058: // asteros
		case 203059: // polinia
		case 203060: // mune
		case 203066: // toleo
		case 203072: // feira
		case 203074: // pranoa
		case 203075: // namus
		case 203079: // melpone
		case 730007: // tree_move_noah
		case 790001: // pernos
		// Ishalgen
		case 203500: // ask
		case 203501: // guheitun
		case 203502: // vanar
		case 203503: // gurt
		case 203504: // vandarnt
		case 203507: // galar
		case 203511: // huggin
		case 203516: // ulgorn
		case 203517: // tobu
		case 203518: // boromer
		case 203519: // nobekk
		case 203522: // kaindal
		case 203523: // glifko
		case 203524: // megin
		case 203525: // sraht
		case 203531: // negi
		case 203532: // df_fisher
		case 203533: // motgar
		case 203534: // davi
		case 203535: // kard
		case 203539: // derot
		case 203540: // mijou
		case 203541: // lidun
		case 203543: // alfrigh
		case 203544: // devalin
		case 203546: // skuld
		case 203547: // erre
		case 203548: // djuefene
		case 203549: // tanmarn
		case 203550: // muninn
		case 203552: // nostla
		case 203589: // npc_moomoobong
		case 203590: // lycan_messenger
		case 790002: // verdandi
		case 790003: // urd
			return true;
		default:
			return false;
	}
}

bool hasQuestInteraction(gameobjects::player::Player& player, gameobjects::Npc& npc) {
	runtime::Ref<templates::quest::QuestNpc> questNpc = questEngine::QuestEngine::getInstance().getQuestNpc(npc.getNpcId());
	if (!questNpc)
		return false;
	for (int32_t startableQuest : questNpc->getOnQuestStart().snapshot()) {
		if (services::QuestService::checkStartConditions(player, startableQuest, false))
			return true;
	}
	for (const runtime::Ptr<questEngine::model::QuestState>& activeQuest : player.getQuestStateList()->getUncompletedQuests()) {
		if (questNpc->getOnTalkEvent().contains(activeQuest->getQuestId()))
			return true;
	}
	return false;
}

} // namespace aion::gameserver::model
