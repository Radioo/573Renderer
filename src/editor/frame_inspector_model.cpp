#include "editor/frame_inspector_model.h"

#include "preset/eval/frame_report.h"

#include <array>
#include <cstdio>
#include <string>
#include <vector>

namespace Editor {

namespace {

std::string Number(float value) {
    std::array<char, 32> buffer = {};
    snprintf(buffer.data(), buffer.size(), "%g", (double)value);
    return buffer.data();
}

std::string FormatReportValue(const Preset::Eval::ReportValue& value) {
    switch (value.kind) {
    case Preset::Eval::ReportKind::Scalar:
        return Number(value.scalar);
    case Preset::Eval::ReportKind::Vector:
        return Number(value.vector[0]) + ", " + Number(value.vector[1]) + ", " +
               Number(value.vector[2]);
    case Preset::Eval::ReportKind::Integer:
        return std::to_string(value.integer);
    case Preset::Eval::ReportKind::Boolean:
        return value.integer != 0 ? "yes" : "no";
    case Preset::Eval::ReportKind::Text:
    default:
        return value.text;
    }
}

}

std::vector<FrameRow> FrameRows(const Preset::Eval::FrameReport& report) {
    std::vector<FrameRow> rows;
    for (const Preset::Eval::ReportEntity& entity : report.entities) {
        rows.push_back(FrameRow{
            .entity = entity.name, .label = entity.name, .value = entity.kind, .header = true});
        for (const Preset::Eval::ReportValue& value : entity.values) {
            rows.push_back(FrameRow{.entity = entity.name,
                                    .label = value.field,
                                    .value = FormatReportValue(value),
                                    .clip = value.clip});
        }
    }
    return rows;
}

}
