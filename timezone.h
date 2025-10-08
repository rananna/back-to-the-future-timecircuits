/**
 * @file timezone.h
 * @brief Contains the data structures and static data for time zone information.
 * @details This file provides the necessary data for handling time zones. It includes a struct
 * for time zone entries, a comprehensive array of supported time zones with their POSIX strings
 * and display names, and a pre-generated JSON string of this data to be sent to the web UI.
 * This approach centralizes time zone data and saves memory by storing large data structures
 * in flash (PROGMEM).
 */
#ifndef TIMEZONE_H
#define TIMEZONE_H

#include <pgmspace.h>

/**
 * @brief A structure to hold all necessary information for a single time zone.
 */
struct TimeZoneEntry {
  const char* tzString;     /**< The POSIX-compliant time zone string used by the `setenv` function. */
  const char* displayName;  /**< A user-friendly name for display in the UI. */
  const char* ianaTzName;   /**< The official IANA time zone name (e.g., "America/New_York"). */
  const char* region;       /**< The geographical region for grouping in the UI. */
};

/**
 * @brief A constant array of `TimeZoneEntry` structs, storing all supported time zones.
 * @details This array is the single source of truth for time zone information in the firmware.
 * It is declared as `const` to ensure it is placed in flash memory, saving RAM.
 */
const TimeZoneEntry TZ_DATA[] = {
	{ "UTC0", "UTC", "Etc/UTC", "Global" },
	{ "NST3:30NDT,M3.2.0,M11.1.0", "Newfoundland (St. John's)", "America/St_Johns", "Americas" },
	{ "AST4ADT,M3.2.0,M11.1.0", "Atlantic (Halifax)", "America/Halifax", "Americas" },
	{ "EST5EDT,M3.2.0,M11.1.0", "Eastern (New York)", "America/New_York", "Americas" },
	{ "CST6CDT,M3.2.0,M11.1.0", "Central (Chicago)", "America/Chicago", "Americas" },
	{ "MST7MDT,M3.2.0,M11.1.0", "Mountain (Denver)", "America/Denver", "Americas" },
	{ "PST8PDT,M3.2.0,M11.1.0", "Pacific (Los Angeles)", "America/Los_Angeles", "Americas" },
	{ "AKST9AKDT,M3.2.0,M11.1.0", "Alaska (Anchorage)", "America/Anchorage", "Americas" },
	{ "MST7", "Mountain (Phoenix, No DST)", "America/Phoenix", "Americas" },
	{ "HST10", "Hawaii (Honolulu, No DST)", "Pacific/Honolulu", "Americas" },
	{ "GMT0BST,M3.5.0/1,M10.5.0", "GMT/BST (London)", "Europe/London", "Europe" },
	{ "CET-1CEST,M3.5.0,M10.5.0", "CET/CEST (Berlin)", "Europe/Berlin", "Europe" },
	{ "EET-2EEST,M3.5.0/3,M10.5.0/4", "EET/EEST (Athens)", "Europe/Athens", "Europe" },
	{ "<+03>-3", "Moscow Standard Time", "Europe/Moscow", "Europe" },
	{ "<+03>-3", "Turkey Time (Istanbul)","Europe/Istanbul", "Europe" },
	{ "IST-5:30", "Indian Standard Time (Kolkata)", "Asia/Kolkata", "Asia" },
	{ "<+08>-8", "Singapore Standard Time", "Asia/Singapore", "Asia" },
	{ "CST-8", "China Standard Time (Shanghai)", "Asia/Shanghai", "Asia" },
	{ "KST-9", "Korea Standard Time (Seoul)", "Asia/Seoul", "Asia" },
	{ "JST-9", "Japan Standard Time (Tokyo)", "Asia/Tokyo", "Asia" },
	{ "<+04>-4", "Gulf Standard Time (Dubai)", "Asia/Dubai", "Asia" },
	{ "AWST-8", "AWST (Perth)", "Australia/Perth", "Australia & Oceania" },
	{ "AEST-10AEDT,M10.1.0,M4.1.0/3", "AEST/AEDT (Sydney)", "Australia/Sydney", "Australia & Oceania" },
	{ "NZST-12NZDT,M9.5.0,M4.1.0/3", "NZST/NZDT (Auckland)", "Pacific/Auckland", "Australia & Oceania" },
	{ "ChST-10", "Chamorro Time (Guam)", "Pacific/Guam", "Australia & Oceania" },
	{ "WAT-1", "West Africa Time (Lagos)", "Africa/Lagos", "Africa" },
	{ "SAST-2", "South Africa Standard Time", "Africa/Johannesburg", "Africa" },
	{ "EET-2","EET (Cairo)", "Africa/Cairo", "Africa" },
	{ "EAT-3", "East Africa Time (Nairobi)", "Africa/Nairobi", "Africa" },
	{ "<-03>3", "Brasilia Time (Sao Paulo)", "America/Sao_Paulo", "South America" },
	{ "<-03>3", "Argentina Time (Buenos Aires)", "America/Argentina/Buenos_Aires", "South America" }
};

/**
 * @brief A macro to calculate the number of time zone options available at compile time.
 */
#define NUM_TIMEZONE_OPTIONS (sizeof(TZ_DATA) / sizeof(TZ_DATA[0]))

/**
 * @brief A pre-generated JSON string of the time zone data, stored in PROGMEM (flash memory).
 * @details This JSON object is sent directly to the web UI to populate the time zone selection
 * dropdown menus. Storing it pre-formatted in flash saves significant RAM and processing time
 * on the device, as it avoids building the JSON string dynamically.
 */
const char TZ_JSON[] PROGMEM = "{\"Global\":[{\"value\":0,\"text\":\"UTC\",\"ianaTzName\":\"Etc/UTC\"}],\"Americas\":[{\"value\":1,\"text\":\"Newfoundland (St. John's)\",\"ianaTzName\":\"America/St_Johns\"},{\"value\":2,\"text\":\"Atlantic (Halifax)\",\"ianaTzName\":\"America/Halifax\"},{\"value\":3,\"text\":\"Eastern (New York)\",\"ianaTzName\":\"America/New_York\"},{\"value\":4,\"text\":\"Central (Chicago)\",\"ianaTzName\":\"America/Chicago\"},{\"value\":5,\"text\":\"Mountain (Denver)\",\"ianaTzName\":\"America/Denver\"},{\"value\":6,\"text\":\"Pacific (Los Angeles)\",\"ianaTzName\":\"America/Los_Angeles\"},{\"value\":7,\"text\":\"Alaska (Anchorage)\",\"ianaTzName\":\"America/Anchorage\"},{\"value\":8,\"text\":\"Mountain (Phoenix, No DST)\",\"ianaTzName\":\"America/Phoenix\"},{\"value\":9,\"text\":\"Hawaii (Honolulu, No DST)\",\"ianaTzName\":\"Pacific/Honolulu\"}],\"Europe\":[{\"value\":10,\"text\":\"GMT/BST (London)\",\"ianaTzName\":\"Europe/London\"},{\"value\":11,\"text\":\"CET/CEST (Berlin)\",\"ianaTzName\":\"Europe/Berlin\"},{\"value\":12,\"text\":\"EET/EEST (Athens)\",\"ianaTzName\":\"Europe/Athens\"},{\"value\":13,\"text\":\"Moscow Standard Time\",\"ianaTzName\":\"Europe/Moscow\"},{\"value\":14,\"text\":\"Turkey Time (Istanbul)\",\"ianaTzName\":\"Europe/Istanbul\"}],\"Asia\":[{\"value\":15,\"text\":\"Indian Standard Time (Kolkata)\",\"ianaTzName\":\"Asia/Kolkata\"},{\"value\":16,\"text\":\"Singapore Standard Time\",\"ianaTzName\":\"Asia/Singapore\"},{\"value\":17,\"text\":\"China Standard Time (Shanghai)\",\"ianaTzName\":\"Asia/Shanghai\"},{\"value\":18,\"text\":\"Korea Standard Time (Seoul)\",\"ianaTzName\":\"Asia/Seoul\"},{\"value\":19,\"text\":\"Japan Standard Time (Tokyo)\",\"ianaTzName\":\"Asia/Tokyo\"},{\"value\":20,\"text\":\"Gulf Standard Time (Dubai)\",\"ianaTzName\":\"Asia/Dubai\"}],\"Australia & Oceania\":[{\"value\":21,\"text\":\"AWST (Perth)\",\"ianaTzName\":\"Australia/Perth\"},{\"value\":23,\"text\":\"NZST/NZDT (Auckland)\",\"ianaTzName\":\"Pacific/Auckland\"},{\"value\":22,\"text\":\"AEST/AEDT (Sydney)\",\"ianaTzName\":\"Australia/Sydney\"},{\"value\":24,\"text\":\"Chamorro Time (Guam)\",\"ianaTzName\":\"Pacific/Guam\"}],\"Africa\":[{\"value\":25,\"text\":\"West Africa Time (Lagos)\",\"ianaTzName\":\"Africa/Lagos\"},{\"value\":26,\"text\":\"South Africa Standard Time\",\"ianaTzName\":\"Africa/Johannesburg\"},{\"value\":27,\"text\":\"EET (Cairo)\",\"ianaTzName\":\"Africa/Cairo\"},{\"value\":28,\"text\":\"East Africa Time (Nairobi)\",\"ianaTzName\":\"Africa/Nairobi\"}],\"South America\":[{\"value\":29,\"text\":\"Brasilia Time (Sao Paulo)\",\"ianaTzName\":\"America/Sao_Paulo\"},{\"value\":30,\"text\":\"Argentina Time (Buenos Aires)\",\"ianaTzName\":\"America/Argentina/Buenos_Aires\"}]}";

#endif // TIMEZONE_H
