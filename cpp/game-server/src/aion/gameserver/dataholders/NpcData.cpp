#include "aion/gameserver/dataholders/NpcData.h"

#include <string>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/dataholders/detail/JavaHashMapOrder.h"
#include "aion/gameserver/dataholders/loadingutils/LoadContext.h"
#include "aion/gameserver/model/DialogAction.h"
#include "aion/gameserver/model/TribeClass.h"
#include "aion/gameserver/model/stats/calc/NpcStatCalculation.h"
#include "aion/gameserver/model/stats/container/StatEnum.h"
#include "aion/gameserver/model/templates/npc/NpcRank.h"
#include "aion/gameserver/model/templates/npc/NpcRating.h"
#include "aion/gameserver/model/templates/npc/NpcTemplate.h"
#include "aion/gameserver/model/templates/stats/StatsTemplate.h"
#include "aion/gameserver/runtime/base/Exceptions.h"

namespace aion::gameserver::dataholders {

using model::stats::calc::NpcStatCalculation;
using model::stats::container::StatEnum;
using model::templates::npc::NpcRank;
using model::templates::npc::NpcRating;
using model::templates::npc::NpcTemplate;
using model::templates::stats::StatsTemplate;

namespace {

/** Java passes the nullable rating and rank to NpcStatCalculation, whose switch throws NullPointerException for null */
template <class E>
E required(const std::optional<E>& value, const char* what) {
	if (!value)
		throw runtime::NullPointerException(std::string("NpcStatCalculation: the npc template's ") + what + " is null");
	return *value;
}

} // namespace

void NpcData::afterUnmarshal(xml::LoadContext& ctx, const xml::XmlParent& /*parent*/) {
	ctx.runAfterUnmarshalTask([this] { init(); });
}

void NpcData::init() {
	static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.dataholders.NpcData");
	detail::JavaHashMapOrder<int32_t, const NpcTemplate*> order;
	for (NpcTemplate& npc : npcs) {
		npcData.insert_or_assign(npc.getTemplateId(), &npc);
		order.put(npc.getTemplateId(), &npc, detail::javaHashCode(npc.getTemplateId()));
		if (npc.getFuncDialogIds() != nullptr) {
			for (int32_t dialogActionId : *npc.getFuncDialogIds()) {
				if (!model::DialogAction::nameOf(dialogActionId))
					log.warn("Unknown dialog action " + std::to_string(dialogActionId) + " for Npc " + std::to_string(npc.getTemplateId()));
			}
		}
		if (npc.getTribe() != model::TribeClass::PET && npc.getTribe() != model::TribeClass::PET_DARK) { // summons and siege weapons have fixed stats
			const std::optional<NpcRating>& rating = npc.getRating();
			const std::optional<NpcRank>& rank = npc.getRank();
			int8_t level = npc.getLevel();
			// the stats belong to the npc template of this unpublished holder
			auto* template_ = const_cast<StatsTemplate*>(npc.getStatsTemplate());
			if (template_ == nullptr)
				throw runtime::NullPointerException("Cannot invoke \"StatsTemplate.getAttack()\" because \"template\" is null (npc " +
				                                    std::to_string(npc.getTemplateId()) + ")");
			auto calculate = [&](StatEnum stat) {
				return NpcStatCalculation::calculateStat(stat, required(rating, "rating"), required(rank, "rank"), level);
			};
			if (template_->getAttack() == 0)
				template_->setAttack(calculate(StatEnum::PHYSICAL_ATTACK));
			if (template_->getAccuracy() == 0)
				template_->setAccuracy(calculate(StatEnum::PHYSICAL_ACCURACY));
			if (template_->getMagicalAttack() == 0)
				template_->setMagicalAttack(calculate(StatEnum::MAGICAL_ATTACK));
			if (template_->getMacc() == 0)
				template_->setMacc(calculate(StatEnum::MAGICAL_ACCURACY));
			if (template_->getMresist() == 0)
				template_->setMresist(calculate(StatEnum::MAGICAL_RESIST));
			if (template_->getMdef() == 0)
				template_->setMdef(calculate(StatEnum::MAGICAL_DEFEND));
			if (template_->getMcrit() == 0)
				template_->setMcrit(50);
			if (template_->getPcrit() == 0)
				template_->setPcrit(10);
			if (template_->getPdef() == 0)
				template_->setPdef(calculate(StatEnum::PHYSICAL_DEFENSE));
			if (template_->getParry() == 0)
				template_->setParry(calculate(StatEnum::PARRY));
			if (level >= 50 && template_->getSpellResist() == 0)
				template_->setSpellResist(calculate(StatEnum::MAGICAL_CRITICAL_RESIST));
			if (level >= 50 && template_->getStrikeResist() == 0) {
				int32_t strikeResist = calculate(StatEnum::PHYSICAL_CRITICAL_RESIST);
				if (strikeResist > 700) // In general strike resist cannot exceed 700 in retail templates, except bosses in Drakenspire Depths
					strikeResist = 700;
				template_->setStrikeResist(strikeResist);
			}
			template_->setStunLikeResistance(calculate(StatEnum::STUNLIKE_RESISTANCE));
		}
		if (npc.getFuncDialogIds() != nullptr)
			functionDialogIds.insert(npc.getFuncDialogIds()->begin(), npc.getFuncDialogIds()->end());
	}
	npcsInHashOrder = order.values();
	// Java: npcs = null (the C++ index points into the storage, which stays)
}

int32_t NpcData::size() const {
	return static_cast<int32_t>(npcData.size());
}

const NpcTemplate* NpcData::getNpcTemplate(int32_t id) const {
	auto it = npcData.find(id);
	return it != npcData.end() ? it->second : nullptr;
}

const std::vector<const NpcTemplate*>& NpcData::getNpcData() const {
	return npcsInHashOrder;
}

bool NpcData::isFunctionDialog(int32_t functionDialogId) const {
	return functionDialogIds.contains(functionDialogId);
}

} // namespace aion::gameserver::dataholders
