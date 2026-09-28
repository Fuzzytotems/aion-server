#include "aion/gameserver/network/aion/clientpackets/AbstractCharacterEditPacket.h"

#include "aion/gameserver/model/Gender.h"
#include "aion/gameserver/model/PlayerClassInfo.h"
#include "aion/gameserver/model/Race.h"
#include "aion/gameserver/model/gameobjects/player/PlayerAppearance.h"
#include "aion/gameserver/utils/Util.h"

namespace aion::gameserver::network::aion::clientpackets {

AbstractCharacterEditPacket::AbstractCharacterEditPacket(int32_t opcode, const StateSet& validStates) : AionClientPacket(opcode, validStates) {
}

AbstractCharacterEditPacket::~AbstractCharacterEditPacket() = default;

void AbstractCharacterEditPacket::readBasicInfo(bool ignoreInvalidPlayerClass) {
	characterName = utils::Util::convertName(readS(25)); // client leaks random data here when entering char creation screen for the first time
	gender = readD() == 0 ? model::Gender::MALE : model::Gender::FEMALE;
	race = readD() == 0 ? model::Race::ELYOS : model::Race::ASMODIANS;
	playerClass = model::getPlayerClassById(static_cast<int8_t>(readD()), ignoreInvalidPlayerClass);
}

void AbstractCharacterEditPacket::readAppearance() {
	playerAppearance = model::gameobjects::player::PlayerAppearance::create();
	model::gameobjects::player::PlayerAppearance& appearance = *playerAppearance;
	appearance.setVoice(readD());
	appearance.setSkinRGB(readD());
	appearance.setHairRGB(readD());
	appearance.setEyeRGB(readD());
	appearance.setLipRGB(readD());
	appearance.setFace(readUC());
	appearance.setHair(readUC());
	appearance.setDeco(readUC());
	appearance.setTattoo(readUC());
	appearance.setFaceContour(readUC());
	appearance.setExpression(readUC());
	readC(); // always 4 o0 // 5 in 1.5.x
	appearance.setJawLine(readUC());
	appearance.setForehead(readUC());
	appearance.setEyeHeight(readUC());
	appearance.setEyeSpace(readUC());
	appearance.setEyeWidth(readUC());
	appearance.setEyeSize(readUC());
	appearance.setEyeShape(readUC());
	appearance.setEyeAngle(readUC());
	appearance.setBrowHeight(readUC());
	appearance.setBrowAngle(readUC());
	appearance.setBrowShape(readUC());
	appearance.setNose(readUC());
	appearance.setNoseBridge(readUC());
	appearance.setNoseWidth(readUC());
	appearance.setNoseTip(readUC());
	appearance.setCheek(readUC());
	appearance.setLipHeight(readUC());
	appearance.setMouthSize(readUC());
	appearance.setLipSize(readUC());
	appearance.setSmile(readUC());
	appearance.setLipShape(readUC());
	appearance.setJawHeigh(readUC());
	appearance.setChinJut(readUC());
	appearance.setEarShape(readUC());
	appearance.setHeadSize(readUC());
	appearance.setNeck(readUC());
	appearance.setNeckLength(readUC());
	appearance.setShoulderSize(readUC());
	appearance.setTorso(readUC());
	appearance.setChest(readUC()); // only woman
	appearance.setWaist(readUC());
	appearance.setHips(readUC());
	appearance.setArmThickness(readUC());
	appearance.setHandSize(readUC());
	appearance.setLegThickness(readUC());
	appearance.setFootSize(readUC());
	appearance.setFacialRate(readUC());
	readC(); // always 0
	appearance.setArmLength(readUC());
	appearance.setLegLength(readUC()); // wrong??
	appearance.setShoulders(readUC()); // 1.5.x May be ShoulderSize
	appearance.setFaceShape(readUC());
	readC();
	readC();
	readC();
	appearance.setHeight(readF());
}

} // namespace aion::gameserver::network::aion::clientpackets
