#include "aion/commons/utils/info/VersionInfo.h"

#include <algorithm>
#include <charconv>

#include "aion/BuildInfo.h"
#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/utils/DateTimeFormatter.h"
#include "aion/commons/utils/StringUtils.h"
#include "aion/commons/utils/info/SystemInfo.h"

namespace aion::commons::utils::info {

namespace {

const logging::Logger& log() {
	static const logging::Logger instance = logging::LoggerFactory::getLogger("com.aionemu.commons.utils.info.VersionInfo");
	return instance;
}

std::optional<std::string> knownValue(std::string_view value) {
	if (value.empty() || value == "unknown")
		return std::nullopt;
	return std::string(value);
}

bool parseNumber(std::string_view text, int& value) {
	auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
	return error == std::errc() && end == text.data() + text.size();
}

/** Parses "yyyy-MM-ddTHH:mm:ss" (local time of the system default zone) or "yyyy-MM-ddTHH:mm:ssZ" (UTC) */
std::optional<std::chrono::sys_seconds> parseBuildDate(std::string_view date) {
	using namespace std::chrono;
	bool utc = date.ends_with('Z');
	if (utc)
		date.remove_suffix(1);
	int y, mo, d, h, mi, s;
	if (date.size() != 19 || date[4] != '-' || date[7] != '-' || date[10] != 'T' || date[13] != ':' || date[16] != ':')
		return std::nullopt;
	if (!parseNumber(date.substr(0, 4), y) || !parseNumber(date.substr(5, 2), mo) || !parseNumber(date.substr(8, 2), d) ||
		!parseNumber(date.substr(11, 2), h) || !parseNumber(date.substr(14, 2), mi) || !parseNumber(date.substr(17, 2), s))
		return std::nullopt;
	year_month_day ymd{year(y), month(static_cast<unsigned>(mo)), day(static_cast<unsigned>(d))};
	if (!ymd.ok() || h > 23 || mi > 59 || s > 59)
		return std::nullopt;
	sys_seconds asUtc = sys_days(ymd) + hours(h) + minutes(mi) + seconds(s);
	if (utc)
		return asUtc;
	// the offset at the local time: first guess with the offset at the same UTC time, then correct it (exact except within DST transitions)
	sys_seconds guess = asUtc - getUtcOffset(asUtc, nullptr);
	return asUtc - getUtcOffset(guess, nullptr);
}

std::string currentBuildTarget() {
	return SystemInfo::getLanguageStandard() + " (" + SystemInfo::getCompilerVersion() + ")";
}

} // namespace

VersionInfo::VersionInfo(std::string source, std::optional<std::string> revision, std::optional<std::string> branch,
	std::optional<std::chrono::sys_seconds> buildDate, std::optional<std::string> buildTarget)
	: source(std::move(source)), revision(std::move(revision)), branch(std::move(branch)), buildDate(buildDate), buildTarget(std::move(buildTarget)) {
}

const VersionInfo& VersionInfo::commons() {
	static const VersionInfo instance = fromBuildInfo("aion_commons", build::REVISION, build::BRANCH, build::DATE);
	return instance;
}

VersionInfo VersionInfo::ofExecutable() {
	std::filesystem::path executable = SystemInfo::getExecutablePath();
	std::u8string fileName = executable.filename().u8string();
	std::string source = fileName.empty() ? "unknown" : std::string(fileName.begin(), fileName.end());
	return fromBuildInfo(std::move(source), build::REVISION, build::BRANCH, build::DATE);
}

VersionInfo VersionInfo::fromBuildInfo(std::string source, std::string_view revision, std::string_view branch, std::string_view date) {
	return VersionInfo(std::move(source), knownValue(revision), knownValue(branch), parseBuildDate(date), currentBuildTarget());
}

std::string VersionInfo::getBuildInfo(const std::chrono::time_zone* timeZone) const {
	std::string buildInfo = !revision ? "" : "revision " + *revision + (!branch ? " " : " (" + *branch + ") ");
	buildInfo += "built on ";
	buildInfo += buildDate ? DateTimeFormatter::ofPattern("yyyy-MM-dd HH:mm").withZone(timeZone).format(*buildDate) : "unknown date";
	if (buildTarget)
		buildInfo += " for " + *buildTarget;
	return buildInfo;
}

std::string VersionInfo::toString() const {
	return toString(nullptr);
}

std::string VersionInfo::toString(const std::chrono::time_zone* timeZone) const {
	return toString(timeZone, 0);
}

std::string VersionInfo::toString(const std::chrono::time_zone* timeZone, size_t sourceLeftPadToWidth) const {
	size_t sourceLength = static_cast<size_t>(StringUtils::utf16Length(source));
	std::string sourceLeftPadded = sourceLeftPadToWidth > sourceLength ? std::string(sourceLeftPadToWidth - sourceLength, ' ') + source : source;
	return sourceLeftPadded + " " + getBuildInfo(timeZone);
}

void VersionInfo::logAll() {
	logAll(ofExecutable(), nullptr);
}

void VersionInfo::logAll(const VersionInfo& versionInfo, const std::chrono::time_zone* timeZone) {
	size_t maxSourceLength = static_cast<size_t>(std::max(StringUtils::utf16Length(commons().source), StringUtils::utf16Length(versionInfo.source)));
	log().info(std::string_view(commons().toString(timeZone, maxSourceLength)));
	log().info(std::string_view(versionInfo.toString(timeZone, maxSourceLength)));
}

} // namespace aion::commons::utils::info
