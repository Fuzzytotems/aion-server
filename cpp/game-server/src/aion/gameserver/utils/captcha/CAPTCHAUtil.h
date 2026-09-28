#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "aion/commons/utils/ByteBuffer.h"
#include "aion/gameserver/utils/captcha/DDSConverter.h"
#include "aion/gameserver/utils/captcha/fwd.h"

namespace aion::gameserver::utils::captcha {

/**
 * C++: a static-only class (fieldmap K5). The word generation is ported; the image needs java.awt text rendering (bold 25 pt Verdana with
 * antialiasing on a 160x80 image), which the port has no replacement for yet (Deviation), so createImage stays unported and createCAPTCHA throws
 * through it (DEVIATIONS); DDSConverter, which turns the image into the DXT1 texture the client shows, is ported.
 *
 * @author Cura
 */
class CAPTCHAUtil {
private:
	static constexpr int32_t DEFAULT_WORD_LENGTH = 6;
	static constexpr std::string_view WORD = "ABCDEFGHIJKLMNOPQRSTUVWXYZ123456789";
	static constexpr int32_t IMAGE_WIDTH = 160;
	static constexpr int32_t IMAGE_HEIGHT = 80;
	static constexpr int32_t TEXT_SIZE = 25;
	static constexpr std::string_view FONT_FAMILY_NAME = "Verdana";

public:
	CAPTCHAUtil() = delete;

	/** create CAPTCHA: the DXT1 texture of the word, null (std::nullopt) if the image could not be drawn */
	static std::optional<commons::utils::ByteBuffer> createCAPTCHA(std::string_view word);

private:
	/** CAPTCHA image create; null (std::nullopt) if drawing failed */
	static std::optional<DDSConverter::Image> createImage(std::string_view word);

public:
	/** @return String random word */
	static std::string getRandomWord();

private:
	/** @return CAPTCHA word */
	static std::string randomWord(int32_t wordLength);
};

} // namespace aion::gameserver::utils::captcha
