// ---------------------------------------------------------------------------
// gui/main.cpp
// Application entry point.
//
// Single-instance enforcement:  a named mutex is created; if it already
// exists the user is notified and the process exits immediately.
//
// Administrator elevation is enforced via the embedded UAC manifest
// (nbi.rc → resources/manifest.xml → requireAdministrator).
// ---------------------------------------------------------------------------

#include <windows.h>
#include <shellapi.h>

#include <QApplication>
#include <QMessageBox>

#include "gui/main_dashboard.hpp"

// ---------------------------------------------------------------------------
// isRunningAsAdmin — checks if current process has admin token
// ---------------------------------------------------------------------------
static bool isRunningAsAdmin()
{
    BOOL is_admin = FALSE;
    PSID admin_group = nullptr;
    SID_IDENTIFIER_AUTHORITY nt_authority = SECURITY_NT_AUTHORITY;

    if (AllocateAndInitializeSid(
            &nt_authority, 2,
            SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS,
            0, 0, 0, 0, 0, 0,
            &admin_group))
    {
        CheckTokenMembership(nullptr, admin_group, &is_admin);
        FreeSid(admin_group);
    }
    return is_admin != FALSE;
}

int main(int argc, char* argv[])
{
    // ------------------------------------------------------------------
    // Admin elevation check (belt-and-suspenders on top of the manifest)
    // The manifest requests elevation, so this should always be true.
    // If somehow not elevated, offer to re-launch as admin.
    // ------------------------------------------------------------------
    if (!isRunningAsAdmin()) {
        const int ans = MessageBoxW(
            nullptr,
            L"Notebook Inspector ต้องการสิทธิ์ Administrator\n\n"
            L"กด Yes เพื่อรีสตาร์ทในฐานะ Administrator\n"
            L"กด No เพื่อออกจากโปรแกรม",
            L"ต้องการสิทธิ์ Administrator",
            MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON1
        );
        if (ans == IDYES) {
            wchar_t path[MAX_PATH];
            GetModuleFileNameW(nullptr, path, MAX_PATH);
            ShellExecuteW(nullptr, L"runas", path, nullptr, nullptr, SW_SHOWNORMAL);
        }
        return 1;
    }

    // ------------------------------------------------------------------
    // Single-instance check
    // ------------------------------------------------------------------
    HANDLE mutex = CreateMutexW(
        nullptr,            // Default security attributes
        TRUE,               // Request immediate ownership
        L"NBI_SingleInstance"
    );

    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        // Another instance is already running — notify and quit.
        // QApplication is not yet constructed, so we use the Win32 API.
        MessageBoxW(
            nullptr,
            L"Notebook Inspector is already running.\n\n"
            L"Check the taskbar or system tray.",
            L"Notebook Inspector",
            MB_OK | MB_ICONINFORMATION
        );
        if (mutex) CloseHandle(mutex);
        return 1;
    }

    // ------------------------------------------------------------------
    // Qt Application
    // ------------------------------------------------------------------
    // High-DPI is on by default in Qt 6; no AA_EnableHighDpiScaling needed.
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("Notebook Inspector"));
    app.setApplicationVersion(QStringLiteral("1.0.0"));
    app.setOrganizationName(QStringLiteral("NBI Project"));
    app.setWindowIcon(QIcon(QStringLiteral(":/icons/app.ico")));

    // Dark palette for a diagnostic-tool aesthetic
    app.setStyle(QStringLiteral("Fusion"));
    QPalette dark_palette;
    dark_palette.setColor(QPalette::Window,          QColor(45,  45,  45));
    dark_palette.setColor(QPalette::WindowText,      QColor(220, 220, 220));
    dark_palette.setColor(QPalette::Base,            QColor(30,  30,  30));
    dark_palette.setColor(QPalette::AlternateBase,   QColor(53,  53,  53));
    dark_palette.setColor(QPalette::ToolTipBase,     QColor(25,  25,  25));
    dark_palette.setColor(QPalette::ToolTipText,     QColor(220, 220, 220));
    dark_palette.setColor(QPalette::Text,            QColor(220, 220, 220));
    dark_palette.setColor(QPalette::Button,          QColor(53,  53,  53));
    dark_palette.setColor(QPalette::ButtonText,      QColor(220, 220, 220));
    dark_palette.setColor(QPalette::BrightText,      Qt::red);
    dark_palette.setColor(QPalette::Highlight,       QColor(42,  130, 218));
    dark_palette.setColor(QPalette::HighlightedText, Qt::black);
    dark_palette.setColor(QPalette::Link,            QColor(42,  130, 218));
    dark_palette.setColor(QPalette::Disabled, QPalette::Text,       QColor(127, 127, 127));
    dark_palette.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(127, 127, 127));
    app.setPalette(dark_palette);

    // ------------------------------------------------------------------
    // Main window
    // ------------------------------------------------------------------
    nbi::MainDashboard dashboard;
    dashboard.show();

    const int exit_code = app.exec();

    // Release the single-instance mutex on clean exit
    if (mutex) {
        ReleaseMutex(mutex);
        CloseHandle(mutex);
    }

    return exit_code;
}
