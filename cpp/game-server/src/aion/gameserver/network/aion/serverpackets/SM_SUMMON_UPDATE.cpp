#include "aion/gameserver/network/aion/serverpackets/SM_SUMMON_UPDATE.h"

#include <memory>
#include <unordered_set>

#include "aion/gameserver/model/gameobjects/Summon.h"
#include "aion/gameserver/model/stats/calc/Stat2.h"
#include "aion/gameserver/model/stats/container/CreatureLifeStats.h"
#include "aion/gameserver/model/stats/container/SummonGameStats.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"
#include "aion/gameserver/network/aion/serverpackets/detail/PacketSupport.h"
#include "aion/gameserver/utils/stats/CalculationType.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_SUMMON_UPDATE::SM_SUMMON_UPDATE(model::gameobjects::Summon& summonValue)
	: AionServerPacket(opcodeOf<SM_SUMMON_UPDATE>), summon(summonValue) {
}

SM_SUMMON_UPDATE::~SM_SUMMON_UPDATE() = default;

void SM_SUMMON_UPDATE::writeImpl(AionConnection* con) {
	using model::stats::calc::Stat2;
	writeC(summon->getLevel());
	writeH(detail::summonModeId(summon->getVisibleMode()));
	writeD(0); // unk
	writeD(0); // unk
	writeD(summon->getLifeStats()->getCurrentHp());
	std::unique_ptr<Stat2> maxHp = summon->getGameStats()->getMaxHp();
	writeD(maxHp->getCurrent());
	std::unique_ptr<Stat2> mainHandPAttack =
		summon->getGameStats()->getMainHandPAttack(std::unordered_set<utils::stats::CalculationType>{utils::stats::CalculationType::DISPLAY});
	writeD(mainHandPAttack->getCurrent());
	std::unique_ptr<Stat2> pDef = summon->getGameStats()->getPDef();
	writeD(pDef->getCurrent());
	std::unique_ptr<Stat2> mResist = summon->getGameStats()->getMResist();
	writeH(mResist->getCurrent());
	std::unique_ptr<Stat2> mDef = summon->getGameStats()->getMDef();
	writeD(mDef->getCurrent());
	std::unique_ptr<Stat2> accuracy = summon->getGameStats()->getMainHandPAccuracy();
	writeH(accuracy->getCurrent());
	std::unique_ptr<Stat2> mainHandPCritical = summon->getGameStats()->getMainHandPCritical();
	writeH(mainHandPCritical->getCurrent());
	std::unique_ptr<Stat2> mBoost = summon->getGameStats()->getMBoost();
	writeH(mBoost->getCurrent());
	std::unique_ptr<Stat2> suppression = summon->getGameStats()->getMBResist();
	writeH(suppression->getCurrent());
	std::unique_ptr<Stat2> mAccuracy = summon->getGameStats()->getMAccuracy();
	writeH(mAccuracy->getCurrent());
	std::unique_ptr<Stat2> mCritical = summon->getGameStats()->getMCritical();
	writeH(mCritical->getCurrent());
	std::unique_ptr<Stat2> parry = summon->getGameStats()->getParry();
	writeH(parry->getCurrent());
	std::unique_ptr<Stat2> evasion = summon->getGameStats()->getEvasion();
	writeH(evasion->getCurrent());
	writeD(maxHp->getBase());
	writeD(mainHandPAttack->getBase());
	writeD(pDef->getBase());
	writeH(mResist->getBase());
	writeD(mDef->getBase());
	writeH(accuracy->getBase());
	writeH(mainHandPCritical->getBase());
	writeH(mBoost->getBase());
	writeH(suppression->getBase());
	writeH(mAccuracy->getBase());
	writeH(mCritical->getBase());
	writeH(parry->getBase());
	writeH(evasion->getBase());
}

} // namespace aion::gameserver::network::aion::serverpackets
