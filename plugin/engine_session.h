#ifndef ATG_ENGINE_SIM_ENGINE_SESSION_H
#define ATG_ENGINE_SIM_ENGINE_SESSION_H

#include <atomic>
#include <string>

class Engine;
class Vehicle;
class Transmission;
class Simulator;

// Headless owner of one compiled engine. The plugin renders it on the audio thread.
class EngineSimSession {
public:
    struct BlockControls {
        float throttle = 0.0f;
        bool ignition = true;
        bool starter = false;
        float clutch = 0.0f;
        int gear = -1;
        float volume = 1.0f;
        float convolution = 1.0f;
        float highFrequencyGain = 0.01f;
        float noise = 1.0f;
        int simulationFrequency = 10000;
        bool hold = false;
        int rpm = 3000;
        bool reset = false;
    };

    struct CompiledEngine {
        Engine *engine = nullptr;
        Vehicle *vehicle = nullptr;
        Transmission *transmission = nullptr;
        std::string scriptPath;
    };

    EngineSimSession();
    ~EngineSimSession();

    // Walks upward from this plugin file, then hint, the process working
    // directory, and ENGINE_SIM_ROOT until it finds assets/main.mr beside es/engine_sim.mr.
    bool loadNear(const std::string &hint);
    bool load(const std::string &assetDirectory, const std::string &libraryDirectory);

    // Compiles a script without touching the live engine. The bundled es/ library,
    // the script's own directory, and the bundled assets directory are search paths.
    // A file that only defines main is imported and called. On success, compiled
    // owns the new objects until install() takes them.
    bool compileScript(const std::string &scriptPath, CompiledEngine &compiled, std::string &error) const;
    // Destroys the live engine and adopts compiled. On failure the previous engine stays.
    bool install(CompiledEngine &compiled);

    void prepare(double sampleRate);
    // Rising edge of reset soft-restarts physics without reloading the script.
    void applyResetEdge(bool reset);
    void process(const BlockControls &controls, int numSamples, float *output);

    bool loaded() const { return m_simulator != nullptr; }
    const std::string &error() const { return m_error; }
    const std::string &scriptPath() const { return m_scriptPath; }
    const std::string &warnings() const { return m_warnings; }
    const std::string &assetDirectory() const { return m_assetDirectory; }

    int simulationFrequency() const { return m_simulationFrequency; }
    float measuredRpm() const { return m_measuredRpm.load(std::memory_order_relaxed); }
    float volume() const { return m_volume; }
    float convolution() const { return m_convolution; }
    float highFrequencyGain() const { return m_highFrequencyGain; }
    float noise() const { return m_noise; }

private:
    static void discard(CompiledEngine &compiled);
    void release();
    void softReset();
    void applyControls(const BlockControls &controls);
    void renderChunk(int numSamples, float *output);

    Engine *m_engine = nullptr;
    Vehicle *m_vehicle = nullptr;
    Transmission *m_transmission = nullptr;
    Simulator *m_simulator = nullptr;

    std::string m_assetDirectory;
    std::string m_libraryDirectory;
    std::string m_scriptPath;
    std::string m_warnings;
    std::string m_error;
    double m_sampleRate = 0.0;
    double m_smoothedThrottle = 0.0;
    int m_appliedGear = -1;
    bool m_crankUntilRunning = false;
    bool m_resetWasHigh = false;

    int m_simulationFrequency = 10000;
    std::atomic<float> m_measuredRpm{0.0f};
    float m_volume = 1.0f;
    float m_convolution = 1.0f;
    float m_highFrequencyGain = 0.01f;
    float m_noise = 1.0f;
};

#endif /* ATG_ENGINE_SIM_ENGINE_SESSION_H */
