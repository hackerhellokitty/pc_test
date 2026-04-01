// ---------------------------------------------------------------------------
// core/report_generator.cpp
// ---------------------------------------------------------------------------
#include "gui/report_generator.hpp"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFont>
#include <QPainter>
#include <QPixmap>
#include <QPrinter>
#include <QScreen>
#include <QApplication>
#include <QRegularExpression>
#include <QPageSize>
#include <QPageLayout>

namespace nbi {

namespace {

// Make a filesystem-safe filename token from serial number
QString safeSerial(const QString& serial) {
    static const QRegularExpression kBadChars(QStringLiteral("[^A-Za-z0-9_-]"));
    return QString(serial).replace(kBadChars, QStringLiteral("_"));
}

// Compute SHA-256 hash for PDF integrity
QString computeHash(const DeviceInfo& device,
                    const std::array<ModuleResult, 10>& results,
                    const QString& secret_key)
{
    QString data = device.serial_number;
    for (const auto& r : results) {
        data += QStringLiteral("|") + QString::number(static_cast<int>(r.status));
    }
    data += QStringLiteral("|") + secret_key;

    const QByteArray hash = QCryptographicHash::hash(
        data.toUtf8(), QCryptographicHash::Sha256
    );
    return QString::fromLatin1(hash.toHex());
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// exportPng
// ---------------------------------------------------------------------------
QString ReportGenerator::exportPng(QWidget* screen_widget,
                                   const DeviceInfo& device,
                                   const ModuleResult& screen_result,
                                   const Options& opts)
{
    // Grab the screen widget
    QPixmap pixmap;
    if (screen_widget) {
        QScreen* screen = QApplication::primaryScreen();
        pixmap = screen->grabWindow(screen_widget->winId());
    }
    if (pixmap.isNull()) {
        pixmap = QPixmap(800, 600);
        pixmap.fill(Qt::black);
    }

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);

    // --- Overlay dead pixel markers (red circles) from issues ---
    // Issues format: "Dead pixel at (x, y)" where x,y are percentages 0-100
    static const QRegularExpression kCoordRe(
        QStringLiteral("at \\((\\d+),\\s*(\\d+)\\)")
    );
    painter.setPen(QPen(Qt::red, 3));
    painter.setBrush(Qt::NoBrush);
    for (const QString& issue : screen_result.issues) {
        auto match = kCoordRe.match(issue);
        if (match.hasMatch()) {
            const int px = static_cast<int>(match.captured(1).toFloat() / 100.f * pixmap.width());
            const int py = static_cast<int>(match.captured(2).toFloat() / 100.f * pixmap.height());
            painter.drawEllipse(px - 10, py - 10, 20, 20);
        }
    }

    // --- Watermark: Serial + Timestamp ---
    const QString ts = QDateTime::currentDateTime().toString(Qt::ISODate);
    const QString watermark = QStringLiteral("S/N: %1  |  %2  |  NBI").arg(
        device.serial_number.isEmpty() ? QStringLiteral("N/A") : device.serial_number,
        ts
    );

    QFont font;
    font.setPixelSize(18);
    font.setBold(true);
    painter.setFont(font);

    // Semi-transparent dark background bar at bottom
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(0, 0, 0, 180));
    painter.drawRect(0, pixmap.height() - 32, pixmap.width(), 32);

    painter.setPen(QColor(200, 200, 200));
    painter.drawText(QRect(8, pixmap.height() - 28, pixmap.width() - 16, 26),
                     Qt::AlignVCenter | Qt::AlignLeft,
                     watermark);

    painter.end();

    // Save
    QDir dir(opts.output_dir);
    if (!dir.exists()) dir.mkpath(QStringLiteral("."));

    const QString fname = QStringLiteral("NBI_Screen_%1_%2.png")
        .arg(safeSerial(device.serial_number))
        .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss")));
    const QString path = dir.filePath(fname);

    if (pixmap.save(path)) return path;
    return {};
}

// ---------------------------------------------------------------------------
// exportPdf
// ---------------------------------------------------------------------------
QString ReportGenerator::exportPdf(const DeviceInfo& device,
                                   const std::array<ModuleResult, 10>& results,
                                   const Options& opts)
{
    QDir dir(opts.output_dir);
    if (!dir.exists()) dir.mkpath(QStringLiteral("."));

    const QString fname = QStringLiteral("NBI_Report_%1_%2.pdf")
        .arg(safeSerial(device.serial_number))
        .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss")));
    const QString path = dir.filePath(fname);

    // Use screen resolution (96 DPI) so point sizes behave predictably
    QPrinter printer(QPrinter::ScreenResolution);
    printer.setOutputFormat(QPrinter::PdfFormat);
    printer.setOutputFileName(path);
    printer.setPageSize(QPageSize(QPageSize::A4));
    printer.setPageMargins(QMarginsF(15, 15, 15, 15), QPageLayout::Millimeter);

    QPainter p;
    if (!p.begin(&printer)) return {};

    const QRect page = printer.pageLayout().paintRectPixels(printer.resolution());
    const int W = page.width();
    int y = 0;

    // Use point sizes — portable across resolutions.
    // LINE height in pixels at screen res: 1pt ≈ 1.33px at 96dpi → 14pt ≈ 19px
    auto makeFont = [](int pt, bool bold) {
        QFont f;
        f.setPointSize(pt);
        f.setBold(bold);
        return f;
    };

    // line_h: single-line height in pixels for a given point size
    auto lineH = [&](int pt) -> int {
        return static_cast<int>(pt * printer.resolution() / 72.0 * 1.5);
    };

    // Draw text; measures actual wrapped height and advances y accordingly.
    // Adds 4px padding below the text block.
    auto drawText = [&](const QString& text, int pt, bool bold,
                        QColor color, int left_px = 0) {
        const QFont f = makeFont(pt, bold);
        p.setFont(f);
        p.setPen(color);
        const int avail_w = W - left_px;
        // Measure actual bounding rect with word-wrap
        const QRect measured = p.boundingRect(
            QRect(left_px, y, avail_w, 10000),
            Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap,
            text);
        // Draw into the measured rect
        p.drawText(measured, Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap, text);
        y += measured.height() + 4;
    };

    auto hline = [&]() {
        y += 6;
        p.setPen(QColor(180, 180, 180));
        p.drawLine(0, y, W, y);
        y += 10;
    };

    // Use dark colours — PDF background is white, text must be dark/coloured
    const QColor kBlack(30, 30, 30);
    const QColor kGray(100, 100, 100);
    const QColor kBlue(21, 101, 192);
    const QColor kGreen(46, 125, 50);
    const QColor kRed(183, 28, 28);
    const QColor kAmber(245, 127, 23);

    // --- Header ---
    drawText(QStringLiteral("Notebook Inspector — ผลการตรวจสภาพ"), 18, true, kBlack);
    drawText(QDateTime::currentDateTime().toString(Qt::ISODate), 10, false, kGray);
    hline();

    // --- Device info ---
    drawText(QStringLiteral("ข้อมูลอุปกรณ์"), 13, true, kBlue);
    const QString sn  = device.serial_number.isEmpty() ? QStringLiteral("N/A") : device.serial_number;
    const QString cpu = device.cpu_name.isEmpty()      ? QStringLiteral("N/A") : device.cpu_name;
    const QString gpu = device.gpu_name.isEmpty()      ? QStringLiteral("N/A") : device.gpu_name;
    const QString ram = device.ram_gb > 0 ? QStringLiteral("%1 GB").arg(device.ram_gb) : QStringLiteral("N/A");
    const QString os  = device.os_version.isEmpty()    ? QStringLiteral("N/A") : device.os_version;
    drawText(QStringLiteral("Serial:  ") + sn,  10, false, kBlack, 16);
    drawText(QStringLiteral("CPU:     ") + cpu, 10, false, kBlack, 16);
    drawText(QStringLiteral("GPU:     ") + gpu, 10, false, kBlack, 16);
    drawText(QStringLiteral("RAM:     ") + ram, 10, false, kBlack, 16);
    drawText(QStringLiteral("OS:      ") + os,  10, false, kBlack, 16);
    hline();

    // --- Module results ---
    static constexpr std::array<const char*, 10> kNames = {
        "Screen","Keyboard","Touchpad","Battery","S.M.A.R.T.",
        "Temperature","Ports","Audio","Network","Physical"
    };

    drawText(QStringLiteral("ผลการทดสอบแต่ละโมดูล"), 13, true, kBlue);
    y += 6;

    for (int i = 0; i < 10; ++i) {
        const auto& r = results[static_cast<std::size_t>(i)];
        QString status_str;
        QColor  status_color;
        switch (r.status) {
            case TestStatus::Pass:    status_str = QStringLiteral("PASS");  status_color = kGreen; break;
            case TestStatus::Fail:    status_str = QStringLiteral("FAIL");  status_color = kRed;   break;
            case TestStatus::Skipped: status_str = QStringLiteral("SKIP");  status_color = kGray;  break;
            default:                  status_str = QStringLiteral("N/A");   status_color = kGray;  break;
        }

        // Measure module line height using boundingRect for consistency
        const QFont font_mod = makeFont(10, false);
        const QString line_text = r.summary.isEmpty()
            ? QString::fromUtf8(kNames[static_cast<std::size_t>(i)])
            : QStringLiteral("%1 — %2")
                .arg(QString::fromUtf8(kNames[static_cast<std::size_t>(i)]), r.summary);

        p.setFont(font_mod);
        const QRect mod_rect = p.boundingRect(
            QRect(68, y, W - 68, 10000),
            Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap,
            line_text);
        const int row_h = mod_rect.height() + 2;

        // Status badge — vertically centred to row
        p.setFont(makeFont(9, true));
        p.setPen(status_color);
        p.drawText(QRect(0, y, 60, row_h), Qt::AlignVCenter | Qt::AlignLeft, status_str);

        // Module name + summary
        p.setFont(font_mod);
        p.setPen(kBlack);
        p.drawText(mod_rect, Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap, line_text);
        y += row_h;

        if (!r.issues.isEmpty()) {
            p.setFont(makeFont(9, false));
            p.setPen(kRed);
            for (const QString& issue : r.issues) {
                const QRect issue_rect = p.boundingRect(
                    QRect(20, y, W - 20, 10000),
                    Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap,
                    QStringLiteral("• ") + issue);
                p.drawText(issue_rect,
                           Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap,
                           QStringLiteral("• ") + issue);
                y += issue_rect.height() + 2;
            }
        }
        y += 2;  // small gap between module rows
    }
    hline();

    // --- SHA-256 hash ---
    const QString hash_str = computeHash(device, results, opts.secret_key.isEmpty()
                                          ? QStringLiteral("NBI-2026") : opts.secret_key);
    drawText(QStringLiteral("SHA-256 Integrity Hash"), 9, true, kGray);
    drawText(hash_str, 8, false, kGray, 8);
    y += 4;
    drawText(QStringLiteral("Hash = SHA256(Serial | ผลแต่ละโมดูล | SecretKey)  —  ตรวจสอบว่า Report ไม่ถูกแก้ไข"),
             8, false, kGray, 8);

    p.end();

    if (QFile::exists(path)) return path;
    return {};
}

} // namespace nbi
