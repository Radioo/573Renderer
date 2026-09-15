#pragma once

#include <cstddef>
#include <cstdint>

namespace AfpLayout {

constexpr std::size_t kAlignment = 4;
constexpr std::size_t kWordPairSize = 4;
constexpr std::size_t kIntSize = 4;
constexpr std::size_t kMaxWordValue = 0xFFFF;
constexpr uint8_t kScrambledStringsMarker = 0x80;

[[nodiscard]] constexpr std::size_t AlignUp(std::size_t value) {
    return (value + kAlignment - 1) & ~(kAlignment - 1);
}

constexpr uint32_t kNativeMagicXor = 0xC1D0B2FF;
constexpr uint32_t kMagicMask = 0x7F7F7F00;
constexpr std::size_t kLengthField = 4;
constexpr std::size_t kDataVersionField = 8;
constexpr std::size_t kNameField = 10;
constexpr std::size_t kFlagsField = 12;
constexpr std::size_t kRectField = 16;
constexpr std::size_t kFpsField = 24;
constexpr std::size_t kBackgroundColourField = 28;
constexpr std::size_t kExportCountField = 32;
constexpr std::size_t kImportCountField = 34;
constexpr std::size_t kRootContainerField = 36;
constexpr std::size_t kExportTableField = 40;
constexpr std::size_t kImportTableField = 44;
constexpr std::size_t kStringTableField = 48;
constexpr std::size_t kStringTableSizeField = 52;
constexpr std::size_t kHeaderSize = 56;
constexpr uint32_t kFlagImportInitializers = 0x4;
constexpr uint32_t kModelledHeaderFlags = 0xCF;
constexpr std::size_t kInitializerEntrySize = 12;

constexpr std::size_t kContainerLabelCountField = 2;
constexpr std::size_t kContainerFrameCountField = 4;
constexpr std::size_t kContainerTagCountField = 8;
constexpr std::size_t kContainerLabelTableField = 12;
constexpr std::size_t kContainerFrameTableField = 16;
constexpr std::size_t kContainerTagTableField = 20;
constexpr std::size_t kContainerHeaderSize = 24;
constexpr uint16_t kContainerScriptLabels = 0x4;
constexpr std::size_t kLabelSize = 4;
constexpr std::size_t kFrameSize = 4;
constexpr uint32_t kFrameFirstTagMask = 0xFFFFF;
constexpr unsigned kFrameCountShift = 20;
constexpr uint32_t kMaxFrameTagCount = 0xFFF;

constexpr std::size_t kTagHeaderSize = 4;
constexpr unsigned kTagCodeShift = 22;
constexpr uint32_t kTagSizeMask = 0x3FFFFF;
constexpr uint32_t kLongTagFlag = 0x1;
constexpr uint16_t kMaxTagCode = 0x3FF;
constexpr uint16_t kTagSprite = 121;
constexpr uint16_t kTagAction = 122;
constexpr uint16_t kTagPlacement = 127;
constexpr uint16_t kTagRemove = 128;
constexpr uint16_t kTagImage = 131;
constexpr uint16_t kTagShape = 132;
constexpr uint16_t kTagCamera = 136;

constexpr uint16_t kSpriteFlags = 1;
constexpr uint32_t kSpriteContainerOffset = 8;
constexpr std::size_t kImageSize = 8;
constexpr uint16_t kCameraPosition = 0x1;
constexpr uint16_t kCameraFocalLength = 0x2;

constexpr uint8_t kBytecodeMarker = 0xFF;
constexpr uint8_t kBytecodeStrings = 0x1;
constexpr std::size_t kBytecodeHeaderSize = 2;

constexpr std::size_t kPlacementHeaderSize = 8;
constexpr uint32_t kPlaceCharacter = 0x2;
constexpr uint32_t kPlaceRatio = 0x10;
constexpr uint32_t kPlaceName = 0x20;
constexpr uint32_t kPlaceClipDepth = 0x40;
constexpr uint32_t kPlaceClipActions = 0x80;
constexpr uint32_t kPlaceScale = 0x100;
constexpr uint32_t kPlaceRotateSkew = 0x200;
constexpr uint32_t kPlaceTranslation = 0x400;
constexpr uint32_t kPlaceMultiplyColour = 0x800;
constexpr uint32_t kPlaceAddColour = 0x1000;
constexpr uint32_t kPlacePackedMultiplyColour = 0x2000;
constexpr uint32_t kPlacePackedAddColour = 0x4000;
constexpr uint32_t kPlaceFilters = 0x10000;
constexpr uint32_t kPlaceBlend = 0x20000;
constexpr uint32_t kPlaceShortScale = 0x40000;
constexpr uint32_t kPlaceShortRotateSkew = 0x80000;
constexpr uint32_t kPlaceClassName = 0x100000;
constexpr uint32_t kPlaceOrigin = 0x1000000;
constexpr uint32_t kPlaceGeometry = 0x2000000;
constexpr uint32_t kPlaceTranslationZ = 0x8000000;
constexpr uint32_t kPlaceMatrix3d = 0x10000000;
constexpr uint32_t kPlaceHsv = 0x20000000;
constexpr uint32_t kPlaceExtended = 0x80000000;
constexpr uint32_t kPlacePresenceBits =
    kPlaceCharacter | kPlaceRatio | kPlaceName | kPlaceClipDepth | kPlaceClipActions | kPlaceScale |
    kPlaceRotateSkew | kPlaceTranslation | kPlaceMultiplyColour | kPlaceAddColour |
    kPlacePackedMultiplyColour | kPlacePackedAddColour | kPlaceFilters | kPlaceBlend |
    kPlaceShortScale | kPlaceShortRotateSkew | kPlaceClassName | kPlaceOrigin | kPlaceGeometry |
    kPlaceTranslationZ | kPlaceMatrix3d | kPlaceHsv | kPlaceExtended;

constexpr uint32_t kExtOriginZ = 0x2;
constexpr uint32_t kExtDiscardedWords = 0x4;
constexpr uint32_t kExtCurves = 0x8;
constexpr uint32_t kExtColourController = 0x10;
constexpr uint32_t kExtGridController = 0x20;
constexpr uint32_t kExtControllerRecord = 0x40;
constexpr uint32_t kExtPresenceBits =
    kExtOriginZ | kExtDiscardedWords | kExtCurves | kExtColourController | kExtGridController;

constexpr std::size_t kClipSizeField = 4;
constexpr std::size_t kClipUnreadWordField = 8;
constexpr std::size_t kClipEventCountField = 10;
constexpr std::size_t kClipHeaderSize = 12;
constexpr std::size_t kClipEventSize = 8;
constexpr std::size_t kClipEventBytesField = 4;
constexpr std::size_t kClipEventCodeField = 6;

constexpr std::size_t kFilterListHeaderSize = 4;
constexpr uint8_t kFilterColourMatrix = 0x6;
constexpr std::size_t kColourMatrixSize = 84;
constexpr std::size_t kColourMatrixHsvSize = 88;
constexpr std::size_t kColourMatrixHeadSize = 4;
constexpr uint8_t kFilterLookup = 0x67;
constexpr std::size_t kLookupHeadSize = 6;
constexpr std::size_t kLookupLengthField = 6;
constexpr std::size_t kLookupUnreadField = 8;
constexpr std::size_t kLookupTableField = 12;
constexpr std::size_t kLookupLengthBase = 8;

constexpr uint16_t kCurveIntValues = 0x1;
constexpr uint16_t kCurveControlPoints = 0x10;
constexpr std::size_t kCurvePointValues = 2;
constexpr std::size_t kCurveControlPointValues = 6;
constexpr unsigned kCurveSlots = 32;
constexpr std::size_t kColourControllerBytes = 4;
constexpr std::size_t kMatrix3dValues = 9;

}
