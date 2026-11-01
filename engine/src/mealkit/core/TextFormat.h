#pragma once

#include "mealkit/core/Types.h"

#include <string>
#include <string_view>

// Format line of MealKit text formats (docs/CODING_MODEL.md "When a text file format changes", contract between tools).
// In every format the first line after the comment header is "@format <kind> <N>":
//   # MealKit table (SoundEvent). Edit it in the editor's [Tables] panel or import a CSV there.
//   @format table 1
// No line means version 1. Writers always write the latest version; when a format changes, bump N and add a migrate on the reading side.
// MealKit tools in other repos also recognize files by this line (even feedback.table/input.bindings that share extensions).
// Formats defined by a tool (MealKit Audio's assets/sounds.events = "audio-events") are read and written by that tool's runtime with the same rules.
namespace mealkit::text {

struct Format {
    const char* kind;
    u32 version; // latest version this engine writes
};

inline constexpr Format kProject{"project", 1};   // mealkit.project
inline constexpr Format kTuning{"tuning", 1};     // game.tuning, view.tuning
inline constexpr Format kSettings{"settings", 1}; // player settings settings.ini
inline constexpr Format kTable{"table", 1};       // *.table, input.bindings, feedback.table
inline constexpr Format kScene{"scene", 4};       // *.scene (version 2: parent-relative values, 3: sky HDRI line, 4: post line. Old "version N" lines are also read)
inline constexpr Format kPrefab{"prefab", 1};   // assets/prefabs/*.prefab (scene entity blocks only, scene/Scene.h savePrefab)
inline constexpr Format kEffect{"effect", 1};     // assets/effects/*.fx (particle effects)
inline constexpr Format kUi{"ui", 1};             // *.ui, theme.ui
inline constexpr Format kStrings{"strings", 1};   // strings/<language>.txt
inline constexpr Format kTool{"tool", 1};         // tool manifest mealkit-tool.txt
inline constexpr Format kDesync{"desync", 1};     // desync dumps desync_*.txt (game/DesyncDump.h, tools\desync_diff.ps1)

// "@format <kind> <N>" (no newline)
std::string formatLine(const Format& format);

// If line (trimmed) starts with "@", reads it as a format line and returns true. Puts the file's version in version.
// Warns with path if the kind differs, the version is newer than this engine, or the shape is wrong (for a newer version, reads only known keys).
bool readFormatLine(std::string_view line, const Format& expected, const std::string& path, u32& version);

// Floats written to text files: the shortest representation that reads back to the same float (0.4, 1e-05). Doesn't become 0.400000006 like %.9g.
// Writes values joined by spaces to out and returns the end (position of the null terminator). 16 bytes per value is enough for out.
char* writeFloats(char* out, size_t capacity, const f32* values, u32 count);
inline char* writeFloat(char* out, size_t capacity, f32 value) { return writeFloats(out, capacity, &value, 1); }

} // namespace mealkit::text
