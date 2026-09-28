#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"

namespace aion::gameserver::model::gameobjects::player {

/**
 * S0c declaration header (docs/design/hub-headers.md §3.5). RefCounted (fieldmap K4, `PlayerAccountData.appearance`), created with create().
 *
 * @author SoulKeeper, srx47, alexa026
 */
class PlayerAppearance : public runtime::RefCounted {
	AION_MAKE_REF_FRIEND
private:
	runtime::Field<int32_t> face{};
	runtime::Field<int32_t> hair{};
	runtime::Field<int32_t> deco{};
	runtime::Field<int32_t> tattoo{};
	runtime::Field<int32_t> faceContour{};
	runtime::Field<int32_t> expression{};
	runtime::Field<int32_t> jawLine{};
	runtime::Field<int32_t> skinRGB{};
	runtime::Field<int32_t> hairRGB{};
	runtime::Field<int32_t> lipRGB{};
	runtime::Field<int32_t> eyeRGB{};
	runtime::Field<int32_t> faceShape{};
	runtime::Field<int32_t> forehead{};
	runtime::Field<int32_t> eyeHeight{};
	runtime::Field<int32_t> eyeSpace{};
	runtime::Field<int32_t> eyeWidth{};
	runtime::Field<int32_t> eyeSize{};
	runtime::Field<int32_t> eyeShape{};
	runtime::Field<int32_t> eyeAngle{};
	runtime::Field<int32_t> browHeight{};
	runtime::Field<int32_t> browAngle{};
	runtime::Field<int32_t> browShape{};
	runtime::Field<int32_t> nose{};
	runtime::Field<int32_t> noseBridge{};
	runtime::Field<int32_t> noseWidth{};
	runtime::Field<int32_t> noseTip{};
	runtime::Field<int32_t> cheek{};
	runtime::Field<int32_t> lipHeight{};
	runtime::Field<int32_t> mouthSize{};
	runtime::Field<int32_t> lipSize{};
	runtime::Field<int32_t> smile{};
	runtime::Field<int32_t> lipShape{};
	runtime::Field<int32_t> jawHeigh{};
	runtime::Field<int32_t> chinJut{};
	runtime::Field<int32_t> earShape{};
	runtime::Field<int32_t> headSize{};
	runtime::Field<int32_t> neck{};
	runtime::Field<int32_t> neckLength{};
	runtime::Field<int32_t> shoulders{};
	runtime::Field<int32_t> shoulderSize{};
	runtime::Field<int32_t> torso{};
	runtime::Field<int32_t> chest{};
	runtime::Field<int32_t> waist{};
	runtime::Field<int32_t> hips{};
	runtime::Field<int32_t> armThickness{};
	runtime::Field<int32_t> armLength{};
	runtime::Field<int32_t> handSize{};
	runtime::Field<int32_t> legThickness{};
	runtime::Field<int32_t> legLength{};
	runtime::Field<int32_t> footSize{};
	runtime::Field<int32_t> facialRate{};
	runtime::Field<int32_t> voice{};
	runtime::Field<float> height{};

protected:
	PlayerAppearance();
	~PlayerAppearance() override;

public:
	/** Java: new PlayerAppearance() */
	static runtime::Ref<PlayerAppearance> create();

	/** Returns character face */
	int32_t getFace() const { return face.get(); }

	/** Sets character's face */
	void setFace(int32_t value) { face.set(value); }

	/** Returns character's hair */
	int32_t getHair() const { return hair.get(); }

	/** Sets charaxcters hair */
	void setHair(int32_t value) { hair.set(value); }

	/** Returns dunno what is this */
	int32_t getDeco() const { return deco.get(); }

	/** Sets some crap, ask Neme what it is */
	void setDeco(int32_t value) { deco.set(value); }

	/** Returns sexy tattoo */
	int32_t getTattoo() const { return tattoo.get(); }

	/**
	 * Set's sexy tattoo.<br>
	 * Not sexy will throw NotSexyTattooException. Just kidding ;)
	 */
	void setTattoo(int32_t value) { tattoo.set(value); }

	int32_t getFaceContour() const { return faceContour.get(); }

	void setFaceContour(int32_t value) { faceContour.set(value); }

	int32_t getExpression() const { return expression.get(); }

	void setExpression(int32_t value) { expression.set(value); }

	int32_t getJawLine() const { return jawLine.get(); }

	void setJawLine(int32_t value) { jawLine.set(value); }

	/** Skin color, let's create pink lesbians :D */
	int32_t getSkinRGB() const { return skinRGB.get(); }

	/** Here is the valid place to make lesbians skin pink */
	void setSkinRGB(int32_t value) { skinRGB.set(value); }

	/** Hair color, personally i prefer brunettes */
	int32_t getHairRGB() const { return hairRGB.get(); }

	/** Sets hair colors. Blonds must pass IQ test ;) */
	void setHairRGB(int32_t value) { hairRGB.set(value); }

	/** Eye colour */
	void setEyeRGB(int32_t value) { eyeRGB.set(value); }

	/** Sets eye colour */
	int32_t getEyeRGB() const { return eyeRGB.get(); }

	/** Lips color. */
	int32_t getLipRGB() const { return lipRGB.get(); }

	/** Sets lips color */
	void setLipRGB(int32_t value) { lipRGB.set(value); }

	/** Returns face shape */
	int32_t getFaceShape() const { return faceShape.get(); }

	/** Sets face shape */
	void setFaceShape(int32_t value) { faceShape.set(value); }

	/** Returns forehead */
	int32_t getForehead() const { return forehead.get(); }

	/** Sets forehead */
	void setForehead(int32_t value) { forehead.set(value); }

	/** Returns eye heigth */
	int32_t getEyeHeight() const { return eyeHeight.get(); }

	/** Sets eye heigth */
	void setEyeHeight(int32_t value) { eyeHeight.set(value); }

	/** Eye space */
	int32_t getEyeSpace() const { return eyeSpace.get(); }

	/** Eye space */
	void setEyeSpace(int32_t value) { eyeSpace.set(value); }

	/** Returns eye width */
	int32_t getEyeWidth() const { return eyeWidth.get(); }

	/** Sets eye width */
	void setEyeWidth(int32_t value) { eyeWidth.set(value); }

	/** Returns eye size. Hentai girls usually have very big eyes */
	int32_t getEyeSize() const { return eyeSize.get(); }

	/**
	 * Set's eye size.<br>
	 * Can be . o O ;)
	 */
	void setEyeSize(int32_t value) { eyeSize.set(value); }

	/** Return eye shape */
	int32_t getEyeShape() const { return eyeShape.get(); }

	/**
	 * Sets Eye shape.<br>
	 * Can be . _ | 0 o O etc :)
	 */
	void setEyeShape(int32_t value) { eyeShape.set(value); }

	/** Return eye angle */
	int32_t getEyeAngle() const { return eyeAngle.get(); }

	/** Sets eye angle, / | \. */
	void setEyeAngle(int32_t value) { eyeAngle.set(value); }

	/** Rerturn brow heigth */
	int32_t getBrowHeight() const { return browHeight.get(); }

	/** Brow heigth */
	void setBrowHeight(int32_t value) { browHeight.set(value); }

	/** Returns brow angle */
	int32_t getBrowAngle() const { return browAngle.get(); }

	/** Sets brow angle */
	void setBrowAngle(int32_t value) { browAngle.set(value); }

	/** Returns brow shape */
	int32_t getBrowShape() const { return browShape.get(); }

	/**
	 * ****************************************************************************************************************
	 * Sets brow shape
	 */
	void setBrowShape(int32_t value) { browShape.set(value); }

	/** Returns nose */
	int32_t getNose() const { return nose.get(); }

	/** Sets nose */
	void setNose(int32_t value) { nose.set(value); }

	/** Returns nose bridge */
	int32_t getNoseBridge() const { return noseBridge.get(); }

	/** Sets nose bridge */
	void setNoseBridge(int32_t value) { noseBridge.set(value); }

	/** Returns nose width */
	int32_t getNoseWidth() const { return noseWidth.get(); }

	/** Sets nose width */
	void setNoseWidth(int32_t value) { noseWidth.set(value); }

	/** Returns noce tip */
	int32_t getNoseTip() const { return noseTip.get(); }

	/** Sets noce tip */
	void setNoseTip(int32_t value) { noseTip.set(value); }

	/** Returns cheeks */
	int32_t getCheek() const { return cheek.get(); }

	/** Sets cheeks */
	void setCheek(int32_t value) { cheek.set(value); }

	/** Returns lip heigth */
	int32_t getLipHeight() const { return lipHeight.get(); }

	/** Sets lip heigth */
	void setLipHeight(int32_t value) { lipHeight.set(value); }

	/** Returns mouth size */
	int32_t getMouthSize() const { return mouthSize.get(); }

	/** Sets mouth size */
	void setMouthSize(int32_t value) { mouthSize.set(value); }

	/** Returns lips size */
	int32_t getLipSize() const { return lipSize.get(); }

	/** Sets lips size */
	void setLipSize(int32_t value) { lipSize.set(value); }

	/** Returns smile */
	int32_t getSmile() const { return smile.get(); }

	/** Sets smile */
	void setSmile(int32_t value) { smile.set(value); }

	/** Returns lips shape */
	int32_t getLipShape() const { return lipShape.get(); }

	/** Sets lips shape */
	void setLipShape(int32_t value) { lipShape.set(value); }

	/** Returns jaws height */
	int32_t getJawHeigh() const { return jawHeigh.get(); }

	/** Sets jaws height */
	void setJawHeigh(int32_t value) { jawHeigh.set(value); }

	/** Returns chin jut */
	int32_t getChinJut() const { return chinJut.get(); }

	/** Sets chin jut */
	void setChinJut(int32_t value) { chinJut.set(value); }

	/** Returns ear shape */
	int32_t getEarShape() const { return earShape.get(); }

	/** Sets ear shape */
	void setEarShape(int32_t value) { earShape.set(value); }

	/** Returns head size */
	int32_t getHeadSize() const { return headSize.get(); }

	/** Sets head size */
	void setHeadSize(int32_t value) { headSize.set(value); }

	/** Returns neck */
	int32_t getNeck() const { return neck.get(); }

	/** Sets neck */
	void setNeck(int32_t value) { neck.set(value); }

	/** Returns neck length */
	int32_t getNeckLength() const { return neckLength.get(); }

	/** Sets neck length, just curious, is it possible to create a giraffe? */
	void setNeckLength(int32_t value) { neckLength.set(value); }

	/** Shoulders */
	int32_t getShoulders() const { return shoulders.get(); }

	/** Shoulders */
	void setShoulders(int32_t value) { shoulders.set(value); }

	/** Shoulder Size */
	int32_t getShoulderSize() const { return shoulderSize.get(); }

	/** Shoulder Size */
	void setShoulderSize(int32_t value) { shoulderSize.set(value); }

	/** Torso */
	int32_t getTorso() const { return torso.get(); }

	/** Sets torso */
	void setTorso(int32_t value) { torso.set(value); }

	/** Returns tits */
	int32_t getChest() const { return chest.get(); }

	/** Sets tits */
	void setChest(int32_t value) { chest.set(value); }

	/** Returns waist */
	int32_t getWaist() const { return waist.get(); }

	/** sets waist */
	void setWaist(int32_t value) { waist.set(value); }

	/** Returns hips */
	int32_t getHips() const { return hips.get(); }

	/** Sets hips */
	void setHips(int32_t value) { hips.set(value); }

	/** Returns arm thickness */
	int32_t getArmThickness() const { return armThickness.get(); }

	/** Sets arm thickness */
	void setArmThickness(int32_t value) { armThickness.set(value); }

	/** Returns arm length */
	int32_t getArmLength() const { return armLength.get(); }

	/** Sets arm length */
	void setArmLength(int32_t value) { armLength.set(value); }

	/** Returns hand size */
	int32_t getHandSize() const { return handSize.get(); }

	/** Sets hand size */
	void setHandSize(int32_t value) { handSize.set(value); }

	/** Returns legs thickness */
	int32_t getLegThickness() const { return legThickness.get(); }

	/** Sets leg thickness */
	void setLegThickness(int32_t value) { legThickness.set(value); }

	/** Returns legs Length */
	int32_t getLegLength() const { return legLength.get(); }

	/** Sets leg length */
	void setLegLength(int32_t value) { legLength.set(value); }

	/** Returns foot size */
	int32_t getFootSize() const { return footSize.get(); }

	/** Sets foot size */
	void setFootSize(int32_t value) { footSize.set(value); }

	/** Retunrs facial rate */
	int32_t getFacialRate() const { return facialRate.get(); }

	/** Sets facial rate */
	void setFacialRate(int32_t value) { facialRate.set(value); }

	/** Returns sexy voice */
	int32_t getVoice() const { return voice.get(); }

	/** Sets sexy voice */
	void setVoice(int32_t value) { voice.set(value); }

	/** Returns height */
	float getHeight() const { return height.get(); }

	float getBoundHeight();

	/** Sets height */
	void setHeight(float value) { height.set(value); }
};

} // namespace aion::gameserver::model::gameobjects::player
