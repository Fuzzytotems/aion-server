#include "aion/gameserver/utils/captcha/CAPTCHAUtil.h"

#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/geoEngine/math/JavaFloat.h"
#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/utils/Exception.h"

#include <exception>
#include <string>

// C++ only, last: the build defines NOGDI for every unit (cmake/AionCompilerOptions.cmake: wingdi.h's ERROR, TRANSPARENT, ... break ported
// identifiers); this unit alone needs GDI for createImage, so it lifts NOGDI and includes wingdi.h after every other header. No ported
// identifier follows.
#if defined(_WIN32)
#undef NOGDI
#include <windows.h>
#include <wingdi.h>
#endif

namespace aion::gameserver::utils::captcha {

// Java CAPTCHAUtil.java:36-39
std::optional<commons::utils::ByteBuffer> CAPTCHAUtil::createCAPTCHA(std::string_view word) {
	std::optional<DDSConverter::Image> bImg = createImage(word);
	return DDSConverter::convertToDxt1NoTransparency(bImg ? &*bImg : nullptr);
}

namespace {

#if defined(_WIN32)
/** Releases a GDI object or device context at scope exit (the Java Graphics2D.dispose) */
template <class H, class F>
struct GdiHandle {
	H handle;
	F release;
	~GdiHandle() {
		if (handle != nullptr)
			release(handle);
	}
};
#endif

} // namespace

// Java CAPTCHAUtil.java:48-84. Java draws with java.awt (Graphics2D); the C++ draws the same picture with GDI: a 160x80 image filled black,
// each char of the word in white bold Verdana of TEXT_SIZE pixels (Java2D's points are pixels) with antialiasing, its baseline at
// x + font.getSize() * i, y + (-1)^i * (TEXT_SIZE / 6). The glyph pixels are GDI's, not Java2D's (DEVIATION: no java.awt in the port; the
// captcha only has to be readable). Without GDI (a non-Windows build) drawing throws, which Java's catch turns into a log line and null.
std::optional<DDSConverter::Image> CAPTCHAUtil::createImage(std::string_view word) {
	std::optional<DDSConverter::Image> bImg;

	try {
#if defined(_WIN32)
		// image create (top-down 32-bit DIB: BGRX bytes, row by row)
		GdiHandle<HDC, decltype(&DeleteDC)> dc{CreateCompatibleDC(nullptr), &DeleteDC};
		if (dc.handle == nullptr)
			throw commons::utils::Exception("CreateCompatibleDC failed");
		BITMAPINFO bmi{};
		bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
		bmi.bmiHeader.biWidth = IMAGE_WIDTH;
		bmi.bmiHeader.biHeight = -IMAGE_HEIGHT;
		bmi.bmiHeader.biPlanes = 1;
		bmi.bmiHeader.biBitCount = 32;
		bmi.bmiHeader.biCompression = BI_RGB;
		void* bits = nullptr;
		GdiHandle<HBITMAP, decltype(&DeleteObject)> bitmap{CreateDIBSection(dc.handle, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0), &DeleteObject};
		if (bitmap.handle == nullptr || bits == nullptr)
			throw commons::utils::Exception("CreateDIBSection failed");
		HGDIOBJ previousBitmap = SelectObject(dc.handle, bitmap.handle);

		// set backgroup color
		RECT all{0, 0, IMAGE_WIDTH, IMAGE_HEIGHT};
		FillRect(dc.handle, &all, static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));

		// set font family, color, size, antialiasing
		const std::wstring family(FONT_FAMILY_NAME.begin(), FONT_FAMILY_NAME.end());
		GdiHandle<HFONT, decltype(&DeleteObject)> font{CreateFontW(-TEXT_SIZE, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_TT_PRECIS,
			CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_SWISS, family.c_str()), &DeleteObject};
		if (font.handle == nullptr)
			throw commons::utils::Exception("CreateFontW failed");
		HGDIOBJ previousFont = SelectObject(dc.handle, font.handle);
		SetTextColor(dc.handle, static_cast<COLORREF>(0x00FFFFFF)); // white (COLORREF 0x00BBGGRR)
		SetBkMode(dc.handle, TRANSPARENT);
		SetTextAlign(dc.handle, TA_BASELINE | TA_LEFT); // Java drawString's y is the baseline

		// word drawing
		int32_t x = 10;
		int32_t y = IMAGE_HEIGHT / 2 + TEXT_SIZE / 2;

		for (size_t i = 0; i < word.size(); i++) {
			const wchar_t ch = static_cast<wchar_t>(static_cast<unsigned char>(word[i]));
			const int32_t sign = i % 2 == 0 ? 1 : -1; // Java: (int) Math.pow(-1, i)
			TextOutW(dc.handle, x + TEXT_SIZE * static_cast<int32_t>(i), y + sign * (TEXT_SIZE / 6), &ch, 1);
		}
		GdiFlush();

		DDSConverter::Image image;
		image.width = IMAGE_WIDTH;
		image.height = IMAGE_HEIGHT;
		image.argb.resize(static_cast<size_t>(IMAGE_WIDTH) * static_cast<size_t>(IMAGE_HEIGHT));
		const auto* pixels = static_cast<const uint32_t*>(bits);
		for (size_t i = 0; i < image.argb.size(); i++)
			image.argb[i] = static_cast<int32_t>(0xFF000000u | (pixels[i] & 0x00FFFFFFu)); // opaque: the image was filled black first

		// resource dispose
		SelectObject(dc.handle, previousFont);
		SelectObject(dc.handle, previousBitmap);
		bImg = std::move(image);
#else
		static_cast<void>(word);
		throw commons::utils::Exception("CAPTCHAUtil.createImage needs GDI (no text rasterizer in this build)");
#endif
	} catch (const std::exception& e) {
		commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.utils.captcha.CAPTCHAUtil").error("", e);
		bImg = std::nullopt;
	}

	return bImg;
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
