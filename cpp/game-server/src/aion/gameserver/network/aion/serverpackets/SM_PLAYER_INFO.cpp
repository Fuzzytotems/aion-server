#include "aion/gameserver/network/aion/serverpackets/SM_PLAYER_INFO.h"

#include <string>
#include <vector>

#include "aion/gameserver/controllers/movement/MovementMask.h"
#include "aion/gameserver/controllers/movement/PlayerMoveController.h"
#include "aion/gameserver/geoEngine/math/Vector3f.h"
#include "aion/gameserver/model/GenderInfo.h"
#include "aion/gameserver/model/PlayerClassInfo.h"
#include "aion/gameserver/model/RaceInfo.h"
#include "aion/gameserver/model/account/Account.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/TransformModel.h"
#include "aion/gameserver/model/gameobjects/player/AbyssRank.h"
#include "aion/gameserver/model/gameobjects/player/CustomPlayerState.h"
#include "aion/gameserver/model/gameobjects/player/Equipment.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/PlayerAppearance.h"
#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"
#include "aion/gameserver/model/gameobjects/player/PlayerSettings.h"
#include "aion/gameserver/model/gameobjects/player/PrivateStore.h"
#include "aion/gameserver/model/house/House.h"
#include "aion/gameserver/model/team/legion/Legion.h"
#include "aion/gameserver/model/team/legion/LegionEmblem.h"
#include "aion/gameserver/model/templates/cp/CPType.h"
#include "aion/gameserver/model/templates/flypath/FlightPath.h"
#include "aion/gameserver/model/templates/housing/HouseAddress.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"
#include "aion/gameserver/network/aion/serverpackets/detail/PacketLookups.h"
#include "aion/gameserver/network/aion/serverpackets/detail/PacketSupport.h"
#include "aion/gameserver/services/conquerorAndProtectorSystem/CPInfo.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_PLAYER_INFO::SM_PLAYER_INFO(model::gameobjects::player::Player& playerValue)
	: SM_PLAYER_INFO(playerValue, false) {
}

SM_PLAYER_INFO::SM_PLAYER_INFO(model::gameobjects::player::Player& playerValue, bool enemyValue)
	: AbstractPlayerInfoPacket(opcodeOf<SM_PLAYER_INFO>), player(playerValue), enemy(enemyValue) {
}

SM_PLAYER_INFO::~SM_PLAYER_INFO() = default;

void SM_PLAYER_INFO::writeImpl(AionConnection* con) {
	using controllers::movement::MovementMask;
	using model::gameobjects::player::CustomPlayerState;
	runtime::Ptr<model::gameobjects::player::Player> activePlayer = detail::requireConnection(con, "SM_PLAYER_INFO").getActivePlayer();
	if (activePlayer == nullptr)
		return;
	runtime::Ptr<model::gameobjects::player::PlayerCommonData> pcd = player->getCommonData();
	runtime::Ptr<model::gameobjects::player::PlayerAppearance> playerAppearance = player->getPlayerAppearance();
	int32_t raceId = activePlayer->isEnemy(*player) ? model::getRaceId(activePlayer->getOppositeRace()) : model::getRaceId(player->getRace());
	if (player->isInCustomState(CustomPlayerState::NEUTRAL_TO_ALL_PLAYERS) || activePlayer->isInCustomState(CustomPlayerState::NEUTRAL_TO_ALL_PLAYERS))
		raceId = model::getRaceId(activePlayer->getRace());
	writeF(player->getX()); // x
	writeF(player->getY()); // y
	writeF(player->getZ()); // z
	writeD(player->getObjectId());
	writeD(pcd->getTemplateId()); // 0xA3 female asmodian, 0xA2 male asmodian, 0xA1 female elyos, 0xA0 male elyos
	writeD(player->getRobotId()); // RobotId
	writeD(player->getTransformModel().getModelId()); // Transformed state: transformed model id, Regular state: player model id
	writeC(0x00); // new 2.0 Packet --- probably pet info?
	writeD(detail::transformTypeId(player->getTransformModel().getType()));
	writeC(enemy ? 0x00 : 0x26);
	writeC(raceId); // race
	writeC(model::getClassId(pcd->getPlayerClass()));
	writeC(model::getGenderId(pcd->getGender())); // sex
	writeH(player->getState());
	writeD(0);
	bool someState = false;
	writeD(someState ? 1 : 0);
	if (someState)
		writeB(std::vector<uint8_t>(13)); // TODO find out what this data controls
	writeC(player->getHeading());
	writeS(player->getName(true));
	writeH(pcd->getTitleId());
	writeH(player->getCommonData()->isHaveMentorFlag() ? 1 : 0);
	writeH(player->getCastingSkillId());
	if (player->isLegionMember()) {
		runtime::Ptr<model::team::legion::Legion> legion = player->getLegion(); // Java calls getLegion() for each field
		writeD(legion->getLegionId());
		writeC(legion->getLegionEmblem()->getEmblemId());
		writeC(detail::legionEmblemTypeValue(legion->getLegionEmblem()->getEmblemType()));
		writeC(legion->getLegionEmblem()->getColor_a());
		writeC(legion->getLegionEmblem()->getColor_r());
		writeC(legion->getLegionEmblem()->getColor_g());
		writeC(legion->getLegionEmblem()->getColor_b());
		writeS(legion->getName());
	} else {
		writeB(std::vector<uint8_t>(12));
	}
	writeC(detail::getHpPercentage(*player));
	writeH(pcd->getDp()); // current dp
	writeC(0x00); // unk (0x00)
	writeEquippedItems(player->getEquipment().getEquippedForAppearance());
	writeD(playerAppearance->getSkinRGB());
	writeD(playerAppearance->getHairRGB());
	writeD(playerAppearance->getEyeRGB());
	writeD(playerAppearance->getLipRGB());
	writeC(playerAppearance->getFace());
	writeC(playerAppearance->getHair());
	writeC(playerAppearance->getDeco());
	writeC(playerAppearance->getTattoo());
	writeC(playerAppearance->getFaceContour());
	writeC(playerAppearance->getExpression());
	writeC(5); // unk 0x05 0x06
	writeC(playerAppearance->getJawLine());
	writeC(playerAppearance->getForehead());
	writeC(playerAppearance->getEyeHeight());
	writeC(playerAppearance->getEyeSpace());
	writeC(playerAppearance->getEyeWidth());
	writeC(playerAppearance->getEyeSize());
	writeC(playerAppearance->getEyeShape());
	writeC(playerAppearance->getEyeAngle());
	writeC(playerAppearance->getBrowHeight());
	writeC(playerAppearance->getBrowAngle());
	writeC(playerAppearance->getBrowShape());
	writeC(playerAppearance->getNose());
	writeC(playerAppearance->getNoseBridge());
	writeC(playerAppearance->getNoseWidth());
	writeC(playerAppearance->getNoseTip());
	writeC(playerAppearance->getCheek());
	writeC(playerAppearance->getLipHeight());
	writeC(playerAppearance->getMouthSize());
	writeC(playerAppearance->getLipSize());
	writeC(playerAppearance->getSmile());
	writeC(playerAppearance->getLipShape());
	writeC(playerAppearance->getJawHeigh());
	writeC(playerAppearance->getChinJut());
	writeC(playerAppearance->getEarShape());
	writeC(playerAppearance->getHeadSize());
	// 1.5.x 0x00, shoulderSize, armLength, legLength (BYTE) after HeadSize
	writeC(playerAppearance->getNeck());
	writeC(playerAppearance->getNeckLength());
	writeC(playerAppearance->getShoulderSize());
	writeC(playerAppearance->getTorso());
	writeC(playerAppearance->getChest()); // only woman
	writeC(playerAppearance->getWaist());
	writeC(playerAppearance->getHips());
	writeC(playerAppearance->getArmThickness());
	writeC(playerAppearance->getHandSize());
	writeC(playerAppearance->getLegThickness());
	writeC(playerAppearance->getFootSize());
	writeC(playerAppearance->getFacialRate());
	writeC(0x00); // always 0
	writeC(playerAppearance->getArmLength());
	writeC(playerAppearance->getLegLength());
	writeC(playerAppearance->getShoulders());
	writeC(playerAppearance->getFaceShape());
	writeC(0x00); // always 0
	writeC(playerAppearance->getVoice());
	writeF(playerAppearance->getHeight());
	writeF(0.25f); // scale
	writeF(2.0f); // gravity or slide surface o_O
	float movementSpeed = detail::getMovementSpeedFloat(*player);
	writeF(movementSpeed);
	detail::AttackSpeedValues attackSpeed = detail::getAttackSpeed(*player);
	writeH(attackSpeed.base);
	writeH(attackSpeed.current);
	writeC(player->getPortAnimationId()); // not visible to other players (they always see a simple fade in animation)
	writeS(player->hasStore() ? player->getStore()->getStoreMessage() : std::string()); // private store message
	runtime::Ptr<controllers::movement::PlayerMoveController> pmc = player->getMoveController();
	int8_t movementMask = pmc->getMovementMask();
	if ((pmc->getMovementMask() & MovementMask::ABSOLUTE) == MovementMask::ABSOLUTE) {
		// calculate the vector, as click to move and target coords are not supported here
		geoEngine::math::Vector3f vector = geoEngine::math::Vector3f(pmc->getTargetX2() - player->getX(), pmc->getTargetY2() - player->getY(),
											   pmc->getTargetZ2() - player->getZ())
											   .normalizeLocal()
											   .multLocal(movementSpeed);
		writeF(vector.getX());
		writeF(vector.getY());
		writeF(vector.getZ());
		movementMask = static_cast<int8_t>(movementMask & ~MovementMask::ABSOLUTE);
	} else {
		writeF(pmc->vectorX.get());
		writeF(pmc->vectorY.get());
		writeF(pmc->vectorZ.get());
	}
	writeF(player->getX());
	writeF(player->getY());
	writeF(player->getZ());
	writeC(movementMask);
	if (player->isUsingFlightTransporterOrWindstream()) {
		writeD(player->getFlightPath()->getId());
		writeD(player->getFlightPath()->getDistance());
	}
	writeC(player->getVisualState()); // visualState
	writeS(player->getCommonData()->getNote()); // note show in right down windows if your target on player
	writeH(player->getLevel()); // [level]
	writeH(player->getPlayerSettings()->getDisplay()); // unk - 0x04
	writeH(player->getPlayerSettings()->getDeny()); // unk - 0x00
	writeH(detail::abyssRankId(player->getAbyssRank()->getRank())); // abyss rank
	writeH(0x00); // unk - 0x01
	runtime::Ptr<model::gameobjects::VisibleObject> target = player->getTarget();
	writeD(target == nullptr ? 0 : target->getObjectId());
	writeC(0); // suspect id
	writeD(player->getCurrentTeamId());
	writeC(player->isMentor() ? 1 : 0);
	runtime::Ptr<model::house::House> activeHouse = detail::getActiveHouse(*player);
	writeD(activeHouse == nullptr ? 0 : activeHouse->getAddress()->getId()); // 3.0
	if (player->getAccount()->getMembership() > 0)
		writeD(0x03 + player->getAccount()->getMembership()); // 1 = normal, 2 = new player(ascension boost), 3 = returning player, 4 = vip 1
	else
		writeD(0x01);
	writeD(0x01); // unk 4.7
	writeC(3); // can be 3 or 5 on elyos side (3 is more common), not sure what it's for (TODO: check asmo side)
	runtime::Ptr<services::conquerorAndProtectorSystem::CPInfo> cpInfo = detail::getCPInfoForCurrentMap(*player);
	using model::templates::cp::CPType;
	writeC(cpInfo != nullptr && cpInfo->getType() == CPType::CONQUEROR ? cpInfo->getRank() : 0); // Conqueror rank
	writeC(cpInfo != nullptr && cpInfo->getType() == CPType::PROTECTOR ? cpInfo->getRank() : 0); // Protector rank
	writeC(0); // Officer rank icon
}

} // namespace aion::gameserver::network::aion::serverpackets
