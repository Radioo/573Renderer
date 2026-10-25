#include "document/script_docs.h"

#include "document/script_source.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace Document {

namespace {

struct Written {
    std::string_view word;
    std::string_view kind;
    std::string_view signature;
    std::string_view summary;
    std::string_view returns;
    std::string_view params;
};

constexpr std::array<Written, 13> kWords{{
    {.word = "call_function",
     .kind = "instruction",
     .signature = "call_function",
     .summary = "Calls the name sitting on top of the stack. The number under the name says how "
                "many arguments follow it, and the call takes the name, the count and the "
                "arguments away and leaves its result in their place.",
     .returns = "whatever the call returns, left on the stack",
     .params = ""},
    {.word = "call_method",
     .kind = "instruction",
     .signature = "call_method",
     .summary = "Calls a method on an object. The stack holds the method name on top, the object "
                "under it, the argument count under that, and then the arguments.",
     .returns = "whatever the method returns, left on the stack",
     .params = ""},
    {.word = "end",
     .kind = "instruction",
     .signature = "end",
     .summary = "Stops the script. The game stops reading here, so anything after it in the tag "
                "is padding rather than instructions.",
     .returns = "nothing",
     .params = ""},
    {.word = "get_variable",
     .kind = "instruction",
     .signature = "get_variable",
     .summary = "Swaps the name on top of the stack for the value it stands for. This is how a "
                "script reaches the aeplib library object before calling a method on it.",
     .returns = "the value, left on the stack",
     .params = ""},
    {.word = "goto_frame2",
     .kind = "instruction",
     .signature = "goto_frame2(flags)",
     .summary = "Jumps to the frame named by the value on top of the stack. When bit 1 of the "
                "flags is set the instruction carries a second number, a bias added to the frame "
                "it lands on.",
     .returns = "nothing",
     .params =
         "flags|one byte|Bit 1 tells the reader that a frame bias follows the flags. The editor "
         "refuses a bias that disagrees with the bit, because that is what the game reads."},
    {.word = "item",
     .kind = "word",
     .signature = "item(type, bytes)",
     .summary = "A pushed value this editor has no nicer spelling for: the push type and its "
                "operand bytes exactly as the file holds them. Writing it back produces the same "
                "bytes it came from.",
     .returns = "the value, pushed",
     .params = "type|push type number|The number the file uses for this kind of "
               "value.;bytes|hex|The operand bytes exactly as the file holds them."},
    {.word = "keep",
     .kind = "word",
     .signature = "keep <call>",
     .summary = "Leaves the call's result on the stack instead of dropping it. The game plays the "
                "same either way, but the bytes differ, so the word exists to write a script back "
                "exactly as it was.",
     .returns = "nothing",
     .params = "call|a call|The call whose result is left where it lands."},
    {.word = "let",
     .kind = "word",
     .signature = "let rN = <call>",
     .summary = "Binds a call's result to one of the script's numbered registers so a later line "
                "can name it. The result also stays on the stack, which is what lets the next "
                "line call a method on the same object.",
     .returns = "nothing",
     .params = "rN|register number|Which register the result is bound to."},
    {.word = "pop",
     .kind = "instruction",
     .signature = "pop",
     .summary = "Throws away the value on top of the stack. Most calls are written with a pop "
                "after them because the script has no use for what they hand back.",
     .returns = "nothing",
     .params = ""},
    {.word = "push",
     .kind = "instruction",
     .signature = "push(value, ...)",
     .summary = "Puts values on the stack, the first one written deepest. Every call reads its "
                "arguments from there, so a push is how a script feeds one.",
     .returns = "the values, on the stack",
     .params = "value|number, text, this, a register or item(...)|Pushed first, so it sits "
               "deepest. A call reads its arguments from the top down."},
    {.word = "set_member",
     .kind = "instruction",
     .signature = "set_member",
     .summary = "Writes one member of an object. The stack holds the object, the member name and "
                "the value, and all three are taken away.",
     .returns = "nothing",
     .params = ""},
    {.word = "store",
     .kind = "instruction",
     .signature = "store rN",
     .summary = "Copies the top of the stack into register N. The value stays on the stack, which "
                "is why a stored result can still be used by the instruction after it.",
     .returns = "nothing",
     .params = "rN|register number|Which of the numbered registers to copy into."},
    {.word = "this",
     .kind = "word",
     .signature = "this",
     .summary = "The object the script runs on, which is the clip that owns the frame. It is "
                "pushed as its own kind of value rather than looked up by name.",
     .returns = "the object, pushed",
     .params = ""},
}};

struct Call {
    std::string_view word;
    std::string_view id;
    std::string_view signature;
    std::string_view summary;
    std::string_view returns;
    std::string_view params;
};

constexpr std::array<Call, 18> kCalls{{
    {.word = "stop",
     .id = "builtin 0x440",
     .signature = "stop(clip)",
     .summary =
         "Stops one clip where it is. Only that clip stops: everything placed inside it keeps "
         "running, which is what deepStop is for. When the clip is one of the AEP wrapper clips (a "
         "clip named aep_dummy) the composition it wraps is stopped with it.",
     .returns = "nothing. The call still leaves one value on the stack and that value is "
                "undefined, which is why the tool writes a pop after it",
     .params =
         "clip|clip object|The clip to work on. A clip is one timeline in the package: the root is "
         "one, and every sprite placed on a depth is another. Scripts usually pass this."},
    {.word = "play",
     .id = "builtin 0x441",
     .signature = "play(clip)",
     .summary = "Lets one clip carry on from where it stopped. The mirror of stop, with the same "
                "rule about children and AEP wrappers.",
     .returns = "nothing. The call still leaves one value on the stack and that value is "
                "undefined, which is why the tool writes a pop after it",
     .params =
         "clip|clip object|The clip to work on. A clip is one timeline in the package: the root is "
         "one, and every sprite placed on a depth is another. Scripts usually pass this."},
    {.word = "gotoAndPlay",
     .id = "builtin 0x442",
     .signature = "gotoAndPlay(clip, frame)",
     .summary = "Seeks a clip to a frame and lets it play on from there. After the seek the "
                "runtime pulls any AEP composition nested under the clip to the matching frame, so "
                "a nested piece does not drift out of step.",
     .returns = "nothing. The call still leaves one value on the stack and that value is "
                "undefined, which is why the tool writes a pop after it",
     .params = "clip|clip object|The clip to work on. A clip is one timeline in the package: the "
               "root is one, and every sprite placed on a depth is another. Scripts usually pass "
               "this.;frame|frame number from 1, or label text|A number is the frame counted from "
               "1, so 1 is the first frame. Anything that is not a number is looked up as a frame "
               "label, and the call is abandoned with a warning when the clip has no such label."},
    {.word = "gotoAndStop",
     .id = "builtin 0x443",
     .signature = "gotoAndStop(clip, frame)",
     .summary = "Seeks a clip to a frame and holds it there. The same frame rule and the same "
                "nested composition re-sync as gotoAndPlay, with the playhead left stopped.",
     .returns = "nothing. The call still leaves one value on the stack and that value is "
                "undefined, which is why the tool writes a pop after it",
     .params = "clip|clip object|The clip to work on. A clip is one timeline in the package: the "
               "root is one, and every sprite placed on a depth is another. Scripts usually pass "
               "this.;frame|frame number from 1, or label text|A number is the frame counted from "
               "1, so 1 is the first frame. Anything that is not a number is looked up as a frame "
               "label, and the call is abandoned with a warning when the clip has no such label."},
    {.word = "deepStop",
     .id = "builtin 0x814",
     .signature = "deepStop(clip)",
     .summary = "Stops this clip and everything under it. Where stop touches one timeline, this "
                "walks the whole subtree and stops each clip in it.",
     .returns = "nothing. The call still leaves one value on the stack and that value is "
                "undefined, which is why the tool writes a pop after it",
     .params =
         "clip|clip object|The clip to work on. A clip is one timeline in the package: the root is "
         "one, and every sprite placed on a depth is another. Scripts usually pass this."},
    {.word = "deepPlay",
     .id = "builtin 0x813",
     .signature = "deepPlay(clip)",
     .summary = "Lets this clip and everything under it carry on. The mirror of deepStop.",
     .returns = "nothing. The call still leaves one value on the stack and that value is "
                "undefined, which is why the tool writes a pop after it",
     .params =
         "clip|clip object|The clip to work on. A clip is one timeline in the package: the root is "
         "one, and every sprite placed on a depth is another. Scripts usually pass this."},
    {.word = "deepGotoAndPlay",
     .id = "builtin 0x815",
     .signature = "deepGotoAndPlay(clip, frame)",
     .summary = "Seeks this clip and its whole subtree to a frame and lets them play on. It uses "
                "the recursive seek rather than the shallow one, so it does not need the extra "
                "nested composition pass that gotoAndPlay does.",
     .returns = "nothing. The call still leaves one value on the stack and that value is "
                "undefined, which is why the tool writes a pop after it",
     .params = "clip|clip object|The clip to work on. A clip is one timeline in the package: the "
               "root is one, and every sprite placed on a depth is another. Scripts usually pass "
               "this.;frame|frame number from 1, or label text|A number is the frame counted from "
               "1, so 1 is the first frame. Anything that is not a number is looked up as a frame "
               "label, and the call is abandoned with a warning when the clip has no such label."},
    {.word = "deepGotoAndStop",
     .id = "builtin 0x816",
     .signature = "deepGotoAndStop(clip, frame)",
     .summary = "Seeks this clip and its whole subtree to a frame and holds them there.",
     .returns = "nothing. The call still leaves one value on the stack and that value is "
                "undefined, which is why the tool writes a pop after it",
     .params = "clip|clip object|The clip to work on. A clip is one timeline in the package: the "
               "root is one, and every sprite placed on a depth is another. Scripts usually pass "
               "this.;frame|frame number from 1, or label text|A number is the frame counted from "
               "1, so 1 is the first frame. Anything that is not a number is looked up as a frame "
               "label, and the call is abandoned with a warning when the clip has no such label."},
    {.word = "goto_play",
     .id = "builtin 0x83b",
     .signature = "goto_play(clip, frame)",
     .summary = "Seeks to a raw frame index and plays on. This is the one family that counts from "
                "0: where gotoAndPlay reads 1 as the first frame, this reads 0. Text is never "
                "looked up as a label here.",
     .returns = "nothing. The call still leaves one value on the stack and that value is "
                "undefined, which is why the tool writes a pop after it",
     .params = "clip|clip object|The clip to work on. A clip is one timeline in the package: the "
               "root is one, and every sprite placed on a depth is another. Scripts usually pass "
               "this.;frame|frame index from 0|The frame counted from 0, passed to the runtime "
               "unchanged."},
    {.word = "goto_stop",
     .id = "builtin 0x83c",
     .signature = "goto_stop(clip, frame)",
     .summary = "Seeks to a raw frame index counted from 0 and holds it there.",
     .returns = "nothing. The call still leaves one value on the stack and that value is "
                "undefined, which is why the tool writes a pop after it",
     .params = "clip|clip object|The clip to work on. A clip is one timeline in the package: the "
               "root is one, and every sprite placed on a depth is another. Scripts usually pass "
               "this.;frame|frame index from 0|The frame counted from 0, passed to the runtime "
               "unchanged."},
    {.word = "goto_play_label",
     .id = "builtin 0x839",
     .signature = "goto_play_label(clip, label)",
     .summary = "Seeks to a frame label and plays on. Always a label, even when the text looks "
                "like a number.",
     .returns = "nothing. The call still leaves one value on the stack and that value is "
                "undefined, which is why the tool writes a pop after it",
     .params = "clip|clip object|The clip to work on. A clip is one timeline in the package: the "
               "root is one, and every sprite placed on a depth is another. Scripts usually pass "
               "this.;label|label text|A frame label of this clip. A missing label warns and the "
               "seek is abandoned."},
    {.word = "goto_stop_label",
     .id = "builtin 0x83a",
     .signature = "goto_stop_label(clip, label)",
     .summary = "Seeks to a frame label and holds it there.",
     .returns = "nothing. The call still leaves one value on the stack and that value is "
                "undefined, which is why the tool writes a pop after it",
     .params = "clip|clip object|The clip to work on. A clip is one timeline in the package: the "
               "root is one, and every sprite placed on a depth is another. Scripts usually pass "
               "this.;label|label text|A frame label of this clip. A missing label warns and the "
               "seek is abandoned."},
    {.word = "deep_goto_play_label",
     .id = "builtin 0x837",
     .signature = "deep_goto_play_label(clip, frame)",
     .summary =
         "The same call as deepGotoAndPlay: the runtime shares one implementation and one frame "
         "rule for both names, so a number still counts from 1 and text is still a label.",
     .returns = "nothing. The call still leaves one value on the stack and that value is "
                "undefined, which is why the tool writes a pop after it",
     .params = "clip|clip object|The clip to work on. A clip is one timeline in the package: the "
               "root is one, and every sprite placed on a depth is another. Scripts usually pass "
               "this.;frame|frame number from 1, or label text|A number is the frame counted from "
               "1, so 1 is the first frame. Anything that is not a number is looked up as a frame "
               "label, and the call is abandoned with a warning when the clip has no such label."},
    {.word = "deep_goto_stop_label",
     .id = "builtin 0x838",
     .signature = "deep_goto_stop_label(clip, frame)",
     .summary = "The same call as deepGotoAndStop, under its other name.",
     .returns = "nothing. The call still leaves one value on the stack and that value is "
                "undefined, which is why the tool writes a pop after it",
     .params = "clip|clip object|The clip to work on. A clip is one timeline in the package: the "
               "root is one, and every sprite placed on a depth is another. Scripts usually pass "
               "this.;frame|frame number from 1, or label text|A number is the frame counted from "
               "1, so 1 is the first frame. Anything that is not a number is looked up as a frame "
               "label, and the call is abandoned with a warning when the clip has no such label."},
    {.word = "aep_set_frame_control",
     .id = "builtin 0x832",
     .signature = "aep_set_frame_control(parent, depth, frame)",
     .summary = "Keeps the instance at a depth out of the picture until its own playhead reaches a "
                "frame. The runtime re-checks it on every seek rather than once, so while that "
                "child sits before frame - 1 it is not drawn at all.",
     .returns = "nothing. The call still leaves one value on the stack and that value is "
                "undefined, which is why the tool writes a pop after it",
     .params = "parent|clip object|The clip the depth belongs to.;depth|depth number|The depth as "
               "the file numbers it. Unlike getInstanceAtDepth, this one adds no bias.;frame|frame "
               "number|The frame the child has to reach before it is drawn."},
    {.word = "aep_set_rect_mask",
     .id = "builtin 0x833",
     .signature = "aep_set_rect_mask(clip, a, b, c, d)",
     .summary =
         "Hangs a rectangle of four numbers on a clip and marks it as rect-masked. When the clip "
         "has a direct child called aep_dummy the rectangle goes on that child instead. Which edge "
         "or size each number is has not been read out of the game: nothing inside afp-core reads "
         "them back, so that meaning lives in the renderer and the editor will not guess it.",
     .returns = "nothing. The call still leaves one value on the stack and that value is "
                "undefined, which is why the tool writes a pop after it",
     .params = "clip|clip object|The clip to work on. A clip is one timeline in the package: the "
               "root is one, and every sprite placed on a depth is another. Scripts usually pass "
               "this.;a|number|One of the four numbers of the rectangle. Their order is "
               "unread.;b|number|One of the four numbers of the rectangle. Their order is "
               "unread.;c|number|One of the four numbers of the rectangle. Their order is "
               "unread.;d|number|One of the four numbers of the rectangle. Their order is unread."},
    {.word = "aep_set_set_frame",
     .id = "builtin 0x836",
     .signature = "aep_set_set_frame(clip, value)",
     .summary = "Stores a number on the clip and does nothing else inside afp-core: no flag, no "
                "seek, no redraw. What reads it back was not found, so this is not the same as "
                "saying it has no effect.",
     .returns = "nothing. The call still leaves one value on the stack and that value is "
                "undefined, which is why the tool writes a pop after it",
     .params = "clip|clip object|The clip to work on. A clip is one timeline in the package: the "
               "root is one, and every sprite placed on a depth is another. Scripts usually pass "
               "this.;value|number|Stored on the clip as it is."},
    {.word = "getInstanceAtDepth",
     .id = "builtin 0x465",
     .signature = "getInstanceAtDepth(depth)",
     .summary = "Finds the instance placed at a depth and hands it back, so the next line can work "
                "on it. This one is a method on a clip rather than an aeplib call, which is why "
                "scripts write it on its own and keep the result in a register. The depth is the "
                "one the timeline shows; the runtime adds the 0x4000 bias itself.",
     .returns = "the instance at that depth, or undefined when nothing is there",
     .params = "depth|depth number|The depth as the timeline shows it. A fractional number warns "
               "and is rounded."},
}};

std::vector<ScriptParam> Split(std::string_view said) {
    std::vector<ScriptParam> out;
    std::size_t at = 0;
    while (at < said.size()) {
        const std::size_t end = std::min(said.find(';', at), said.size());
        const std::string_view one = said.substr(at, end - at);
        const std::size_t first = one.find('|');
        const std::size_t second = one.find('|', first + 1);
        if (first != std::string_view::npos && second != std::string_view::npos) {
            out.push_back(
                ScriptParam{.name = std::string(one.substr(0, first)),
                            .type = std::string(one.substr(first + 1, second - first - 1)),
                            .said = std::string(one.substr(second + 1))});
        }
        at = end + 1;
    }
    return out;
}

std::optional<ScriptDoc> Plain(const Written& said) {
    return ScriptDoc{.name = std::string(said.word),
                     .kind = std::string(said.kind),
                     .signature = std::string(said.signature),
                     .summary = std::string(said.summary),
                     .returns = std::string(said.returns),
                     .params = Split(said.params),
                     .id = {},
                     .call = false,
                     .measured = true};
}

std::string Hex(uint16_t id) {
    std::string digits;
    for (int shift = 12; shift >= 0; shift -= 4) {
        const auto nibble = static_cast<uint8_t>((id >> shift) & 0xF);
        if (digits.empty() && nibble == 0 && shift > 0) continue;
        digits += "0123456789abcdef"[nibble];
    }
    return "builtin 0x" + digits;
}

}

std::optional<ScriptDoc> ScriptWordDoc(std::string_view word) {
    const auto found = std::ranges::find(kWords, word, &Written::word);
    if (found != kWords.end()) return Plain(*found);

    const auto called = std::ranges::find(kCalls, word, &Call::word);
    if (called != kCalls.end()) {
        return ScriptDoc{.name = std::string(word),
                         .kind = "call",
                         .signature = std::string(called->signature),
                         .summary = std::string(called->summary),
                         .returns = std::string(called->returns),
                         .params = Split(called->params),
                         .id = std::string(called->id),
                         .call = true,
                         .measured = true};
    }

    const std::optional<uint16_t> id = ScriptCallId(word);
    if (!id) return std::nullopt;
    return ScriptDoc{
        .name = std::string(word),
        .kind = "call",
        .signature = std::string(word) + "(...)",
        .summary = "A call afp-core knows by name. The bytecode stores it as the number beside it "
                   "rather than as text, so the game resolves it by that id. What this one does "
                   "has not been read out of the game yet, so the editor will not guess.",
        .returns = "unread",
        .params = {},
        .id = Hex(*id),
        .call = true,
        .measured = false};
}

}
