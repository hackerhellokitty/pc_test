#pragma once
// ---------------------------------------------------------------------------
// gui/report_generator.hpp
// Generates PNG screenshot (with watermark) and PDF report (with SHA-256 hash).
// ---------------------------------------------------------------------------
#include <array>
#include <QString>
#include <QWidget>
#include "common/types.hpp"

namespace nbi {

class ReportGenerator {
public:
    struct Options {
        QString      output_dir;        ///< directory to save files
        QString      secret_key;        ///< for PDF hash (default "NBI-2026")
    };

    /// Generate PNG: grab screen from widget, overlay dead pixels from screen result,
    /// watermark with serial + timestamp, save as PNG.
    /// Returns saved file path or empty on failure.
    static QString exportPng(QWidget* screen_widget,
                             const DeviceInfo& device,
                             const ModuleResult& screen_result,
                             const Options& opts);

    /// Generate PDF: full report layout with all module results + SHA-256 hash.
    /// Returns saved file path or empty on failure.
    static QString exportPdf(const DeviceInfo& device,
                             const std::array<ModuleResult, 10>& results,
                             const Options& opts);
};

} // namespace nbi
