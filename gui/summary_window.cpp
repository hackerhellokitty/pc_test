// ---------------------------------------------------------------------------
// gui/summary_window.cpp
// ---------------------------------------------------------------------------
#include "gui/summary_window.hpp"
#include "gui/summary_page.hpp"
#include "gui/report_generator.hpp"

#include <QFileDialog>
#include <QMessageBox>
#include <QVBoxLayout>

namespace nbi {

SummaryWindow::SummaryWindow(const DeviceInfo& device,
                             const std::array<ModuleResult, 10>& results,
                             QWidget* screen_widget,
                             QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("สรุปผลการตรวจสภาพ — Notebook Inspector"));
    setMinimumSize(600, 540);
    setAttribute(Qt::WA_DeleteOnClose);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    m_page = new SummaryPage(this);
    m_page->setData(device, results);
    layout->addWidget(m_page);

    connect(m_page, &SummaryPage::exportRequested, this,
            [this, device, results, screen_widget]() {

        const QString dir = QFileDialog::getExistingDirectory(
            this,
            QStringLiteral("เลือกโฟลเดอร์สำหรับบันทึก Report"),
            QStringLiteral("C:/")
        );
        if (dir.isEmpty()) return;

        ReportGenerator::Options opts;
        opts.output_dir = dir;
        opts.secret_key = QStringLiteral("NBI-2026");

        // --- PDF ---
        const QString pdf_path = ReportGenerator::exportPdf(device, results, opts);

        // --- PNG (screen capture + dead pixel overlay + watermark) ---
        // results[0] = Screen module — issues contain dead pixel coordinates
        const QString png_path = ReportGenerator::exportPng(
            screen_widget, device, results[0], opts);

        // Build result message
        QStringList saved;
        if (!pdf_path.isEmpty()) saved << QStringLiteral("PDF: ") + pdf_path;
        if (!png_path.isEmpty()) saved << QStringLiteral("PNG: ") + png_path;

        if (!saved.isEmpty()) {
            QMessageBox::information(
                this,
                QStringLiteral("Export สำเร็จ"),
                QStringLiteral("บันทึกไฟล์แล้ว:\n") + saved.join(QLatin1Char('\n'))
            );
        } else {
            QMessageBox::warning(
                this,
                QStringLiteral("Export ล้มเหลว"),
                QStringLiteral("ไม่สามารถสร้างไฟล์ได้ ตรวจสอบสิทธิ์การเขียน")
            );
        }
    });
}

} // namespace nbi
