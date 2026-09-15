#include "aion/gameserver/utils/captcha/CAPTCHAUtil.h"

#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/geoEngine/math/JavaFloat.h"
#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::utils::captcha {

std::optional<commons::utils::ByteBuffer> CAPTCHAUtil::createCAPTCHA(std::string_view word) {
	std::optional<DDSConverter::Image> bImg = createImage(word);
	return DDSConverter::convertToDxt1NoTransparency(bImg ? &*bImg : nullptr);
}

std::optional<DDSConverter::Image> CAPTCHAUtil::createImage(std::string_view word) {
	// Java: a TYPE_INT_ARGB_PRE image of IMAGE_WIDTH x IMAGE_HEIGHT filled black, each char of the word drawn in white with
	// Font(FONT_FAMILY_NAME, Font.BOLD, TEXT_SIZE) and text antialiasing at x = 10 + TEXT_SIZE * i, y = IMAGE_HEIGHT / 2 + TEXT_SIZE / 2 +
	// (-1)^i * (TEXT_SIZE / 6); on an exception it logs and returns null. No text rasterizer exists in the port (class comment).
	AION_UNPORTED();
}

std::string CAPTCHAUtil::getRandomWord() {
	return randomWord(DEFAULT_WORD_LENGTH);
}

std::string CAPTCHAUtil::randomWord(int32_t wordLength) {
	std::string word;
	for (int32_t i = 0; i < wordLength; i++) {
		// Java: Math.abs((int) (Math.random() * WORD.length())) - Math.random() is the commons Rnd generator here
		int32_t index = geoEngine::math::JavaFloat::doubleToInt(commons::utils::Rnd::nextDouble() * static_cast<double>(WORD.size()));
		index = index < 0 ? -index : index;
		word += WORD[static_cast<size_t>(index)];
	}
	return word;
}

} // namespace aion::gameserver::utils::captcha
