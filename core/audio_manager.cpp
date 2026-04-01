// ---------------------------------------------------------------------------
// core/audio_manager.cpp
// ---------------------------------------------------------------------------

#include "core/audio_manager.hpp"

#include <QMediaDevices>
#include <QAudioDevice>

namespace nbi {

// ---------------------------------------------------------------------------
// enumerateOutputs
// ---------------------------------------------------------------------------
std::vector<AudioDevice> AudioManager::enumerateOutputs()
{
    std::vector<AudioDevice> result;

    const QAudioDevice defaultDev = QMediaDevices::defaultAudioOutput();
    const QList<QAudioDevice> devs = QMediaDevices::audioOutputs();

    for (const QAudioDevice& d : devs) {
        AudioDevice ad;
        ad.id         = d.id();
        ad.name       = d.description();
        ad.is_default = (d.id() == defaultDev.id());
        result.push_back(std::move(ad));
    }

    return result;
}

// ---------------------------------------------------------------------------
// evaluate
// ---------------------------------------------------------------------------
ModuleResult AudioManager::evaluate(const AudioResult& r)
{
    ModuleResult m;
    m.label = QStringLiteral("Audio");

    if (r.no_device) {
        m.status  = TestStatus::Skipped;
        m.summary = QStringLiteral("ไม่พบลำโพง");
        return m;
    }

    if (r.outcomes.empty()) {
        m.status  = TestStatus::NotRun;
        m.summary = QStringLiteral("ยังไม่ได้ทดสอบ");
        return m;
    }

    int failed = 0;
    for (const auto& o : r.outcomes) {
        if (!o.heard) {
            ++failed;
            // Find device name
            for (const auto& dev : r.outputs) {
                if (dev.id == o.device_id)
                    m.issues.append(QStringLiteral("ไม่ได้ยินเสียงจาก: %1").arg(dev.name));
            }
        }
    }

    if (failed == 0) {
        m.status  = TestStatus::Pass;
        m.summary = QStringLiteral("ลำโพงปกติ");
    } else {
        m.status  = TestStatus::Fail;
        m.summary = QStringLiteral("ไม่ได้ยิน %1/%2 อุปกรณ์")
            .arg(failed).arg(r.outcomes.size());
    }

    return m;
}

} // namespace nbi
