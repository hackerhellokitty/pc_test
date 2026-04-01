#pragma once

// ---------------------------------------------------------------------------
// core/audio_manager.hpp
// Audio output (speaker) enumeration. Tone generation is done in GUI layer
// using QAudioSink directly so the device handle stays on the GUI thread.
// ---------------------------------------------------------------------------

#include <vector>
#include <QString>
#include "common/types.hpp"

namespace nbi {

struct AudioDevice {
    QString id;            ///< QAudioDevice::id()
    QString name;          ///< Friendly name
    bool    is_default{false};
};

struct AudioResult {
    std::vector<AudioDevice> outputs;   ///< Enumerated output devices
    bool   no_device{false};            ///< No output device found
    // Per-device test outcome set by GUI after user confirms
    struct DeviceOutcome {
        QString device_id;
        bool    heard{false};           ///< User confirmed heard sound
    };
    std::vector<DeviceOutcome> outcomes;
};

class AudioManager {
public:
    static std::vector<AudioDevice> enumerateOutputs();
    static ModuleResult             evaluate(const AudioResult& r);
};

} // namespace nbi
