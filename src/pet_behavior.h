#pragma once
#include <behaviortree_cpp/bt_factory.h>
#include <behaviortree_cpp/xml_parsing.h>
#include <behaviortree_cpp/loggers/bt_file_logger_v2.h>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <functional>
#include <memory>
#include <random>
#include <sstream>
#include <optional>

namespace pet {
inline constexpr double MovementSpeedMultiplier = 2.5;
struct Clip { std::string name; double duration; int modelIndex; };

// Animation state is independent of raylib, so priority/cancellation can be tested
// using a deterministic clock without creating a desktop window.
struct Playback {
    std::vector<Clip> clips;
    std::mt19937 random{std::random_device{}()};
    std::function<void(const std::string&)> log = [](const std::string&) {};
    std::function<bool(double, double)> canWalk = [](double, double) { return true; };
    int current = -1;
    double time = 0, idleTime = 0, reaction = 0;
    bool active = false, loop = false, pendingClick = false, dragging = false;
    bool walking = false;
    double walkYaw = 0, walkSpeed = 0, walkRemaining = 0;
    double inactiveSeconds = 0, movementScale = 1, cursorYaw = 0;
    bool cursorTarget = false;
    double lookYaw = 0;
    bool lookTarget = false;
    bool walkEnabled = true, trackingEnabled = true, settingsOpen = false, outlineEnabled = false;
    bool returnRequested = false, returningHome = false, returnArrived = false;
    std::optional<double> idleSecondsOverride, activeSecondsOverride;
    std::optional<double> walkIntervalOverride,walkDurationOverride,attentionProbabilityOverride;
    double walkTimeJitter=0,walkDistanceJitter=0; // Opt-in Three.js variation around configured values.
    double inactivityThreshold() const { return idleSecondsOverride.value_or(60.0); }
    bool inputActive = false;
    bool noInputState = false, trackingCursor = false;
    double activeInputSeconds = 0;
    uint64_t inputEpoch = 0, attentionAttemptEpoch = UINT64_MAX;
    std::optional<double> facingRequest;
    uint64_t generation = 0, cooldownEpoch = 0;

    bool observeInput(double secondsSinceInput, double dt) {
        inactiveSeconds = secondsSinceInput;
        const bool activeNow = secondsSinceInput <= 3.0;
        if (secondsSinceInput >= inactivityThreshold()) { noInputState = true; trackingCursor = false; }
        const bool resumed = activeNow && noInputState;
        if (resumed) noInputState = false;
        if (activeNow && !inputActive) { ++inputEpoch; activeInputSeconds = 0; }
        else if (activeNow) activeInputSeconds += dt;
        else activeInputSeconds = 0;
        inputActive = activeNow;
        return resumed;
    }

    int find(const std::string& name) const {
        for (size_t i = 0; i < clips.size(); ++i) if (clips[i].name == name) return static_cast<int>(i);
        throw std::runtime_error("Unknown animation: " + name);
    }
    uint64_t start(int clip, bool looping, bool moving = false) {
        current = clip; time = 0; active = true; loop = looping;
        walking = moving;
        if (!looping) reaction = 0.7;
        ++generation;
        log(std::string("ACTION ") + (moving ? "walk: " : looping ? "idle: " : "play: ") + clips.at(clip).name);
        return generation;
    }
    void stop(uint64_t owner) {
        // ReactiveFallback starts the new branch BEFORE halting the previous one.
        // An old node must never stop the new node's playback.
        if (generation == owner) { active = false; walking = false; }
    }
    void resetCooldown() { idleTime = 0; ++cooldownEpoch; }
    void advance(double dt) {
        reaction = std::max(0.0, reaction - dt);
        if (!active) return;
        time += dt;
        if (walking) walkRemaining = std::max(0.0, walkRemaining - dt);
        if (loop) {
            if (!dragging && !walking) idleTime += dt;
            time = std::fmod(time, clips.at(current).duration);
        }
    }
};

template<class T> T input(const BT::TreeNode& node, const char* port) {
    auto value = node.getInput<T>(port);
    if (!value) throw std::runtime_error(node.name() + ": " + value.error());
    return value.value();
}

inline std::vector<int> clipList(const Playback& state, const std::string& text) {
    std::vector<int> result;
    std::istringstream stream(text);
    std::string name;
    while (std::getline(stream, name, ';')) {
        const auto first = name.find_first_not_of(" \t\r\n"), last = name.find_last_not_of(" \t\r\n");
        if (first == std::string::npos) throw std::runtime_error("Empty animation name in clips");
        result.push_back(state.find(name.substr(first, last - first + 1)));
    }
    if (result.empty()) throw std::runtime_error("clips must contain at least one animation");
    return result;
}

class IdleCooldownReady : public BT::ConditionNode {
    Playback& state;
    uint64_t epoch = UINT64_MAX;
    double target = 0;
public:
    IdleCooldownReady(const std::string& name, const BT::NodeConfig& config, Playback& s)
        : ConditionNode(name, config), state(s) {}
    static BT::PortsList providedPorts() {
        return {BT::InputPort<double>("min_seconds", 5.0, "Minimum idle delay"),
                BT::InputPort<double>("max_seconds", 12.0, "Maximum idle delay")};
    }
    BT::NodeStatus tick() override {
        if (epoch != state.cooldownEpoch) {
            target = state.walkIntervalOverride.value_or(std::uniform_real_distribution<double>(input<double>(*this, "min_seconds"),
                                                           input<double>(*this, "max_seconds"))(state.random));
            if(state.walkIntervalOverride&&state.walkTimeJitter>0)target*=std::uniform_real_distribution<double>(1-state.walkTimeJitter,1+state.walkTimeJitter)(state.random);
            epoch = state.cooldownEpoch;
        }
        return state.idleTime >= target ? BT::NodeStatus::SUCCESS : BT::NodeStatus::FAILURE;
    }
};

// Three registered leaf types share the same playback/cancellation implementation.
class AnimationAction : public BT::StatefulActionNode {
    Playback& state;
    std::string kind;
    uint64_t owner = 0;
    bool reset = false;
public:
    AnimationAction(const std::string& name, const BT::NodeConfig& config, Playback& s, std::string type)
        : StatefulActionNode(name, config), state(s), kind(std::move(type)) {}
    BT::NodeStatus onStart() override {
        const bool looping = kind == "LoopAnimation";
        int clip;
        if (kind == "PlayRandomAnimation") {
            const auto list = clipList(state, input<std::string>(*this, "clips"));
            clip = list[std::uniform_int_distribution<size_t>(0, list.size() - 1)(state.random)];
        } else clip = state.find(input<std::string>(*this, "clip"));
        reset = !looping && input<bool>(*this, "reset_cooldown");
        if (kind == "PlayAnimation" && input<bool>(*this, "consume_click")) state.pendingClick = false;
        owner = state.start(clip, looping);
        return BT::NodeStatus::RUNNING;
    }
    BT::NodeStatus onRunning() override {
        if (owner != state.generation) return BT::NodeStatus::FAILURE;
        if (kind == "LoopAnimation" || state.time < state.clips.at(state.current).duration)
            return BT::NodeStatus::RUNNING;
        state.stop(owner);
        if (reset) state.resetCooldown();
        return BT::NodeStatus::SUCCESS;
    }
    void onHalted() override {
        state.stop(owner);
        if (reset) state.resetCooldown();
    }
};

class WalkRandom : public BT::StatefulActionNode {
    Playback& state;
    uint64_t owner = 0;
public:
    WalkRandom(const std::string& name, const BT::NodeConfig& config, Playback& s)
        : StatefulActionNode(name, config), state(s) {}
    static BT::PortsList providedPorts() {
        return {BT::InputPort<std::string>("clip", "Cafe_Walk", "Looping walk animation"),
                BT::InputPort<double>("speed", 0.792, "Ground units per second"),
                BT::InputPort<double>("min_seconds", 2.0, "Minimum walk duration"),
                BT::InputPort<double>("max_seconds", 4.0, "Maximum walk duration"),
                BT::InputPort<double>("up_weight", 0.4, "Relative probability of away/up headings"),
                BT::InputPort<double>("up_distance", 2.5, "Duration multiplier for away/up headings"),
                BT::InputPort<double>("cursor_bias", 2.0, "Extra weight toward cursor; zero disables")};
    }
    BT::NodeStatus onStart() override {
        double duration = state.walkDurationOverride.value_or(std::uniform_real_distribution<double>(input<double>(*this, "min_seconds"),
                                                                       input<double>(*this, "max_seconds"))(state.random));
        if(state.walkDurationOverride&&state.walkDistanceJitter>0)duration*=std::uniform_real_distribution<double>(1-state.walkDistanceJitter,1+state.walkDistanceJitter)(state.random);
        const double speed = input<double>(*this, "speed");
        std::vector<int> directions;
        std::vector<double> weights, durations;
        for (int yaw = 0; yaw < 360; yaw += 45) {
            const bool upward = yaw > 90 && yaw < 270;
            const double seconds = duration*(upward ? input<double>(*this, "up_distance") : 1.0);
            if (!state.canWalk(yaw, speed*seconds*state.movementScale)) continue;
            double weight = upward ? input<double>(*this, "up_weight") : 1.0;
            if (state.cursorTarget) {
                const double alignment = std::max(0.0, std::cos((yaw - state.cursorYaw)*3.141592653589793/180));
                weight *= 1 + input<double>(*this, "cursor_bias")*alignment*alignment;
            }
            directions.push_back(yaw); durations.push_back(seconds); weights.push_back(weight);
        }
        if (directions.empty()) { state.resetCooldown(); return BT::NodeStatus::FAILURE; }
        const auto selected = std::discrete_distribution<size_t>(weights.begin(), weights.end())(state.random);
        const int yaw = directions[selected];
        owner = state.start(state.find(input<std::string>(*this, "clip")), true, true);
        state.walkYaw = yaw; state.walkSpeed = speed; state.walkRemaining = durations[selected];
        state.log("WALK heading=" + std::to_string(yaw) + " seconds=" + std::to_string(state.walkRemaining));
        return BT::NodeStatus::RUNNING;
    }
    BT::NodeStatus onRunning() override {
        if (owner != state.generation) return BT::NodeStatus::FAILURE;
        if (state.walkRemaining > 0) return BT::NodeStatus::RUNNING;
        state.stop(owner); state.resetCooldown();
        return BT::NodeStatus::SUCCESS;
    }
    void onHalted() override { state.stop(owner); state.resetCooldown(); }
};

class ReturnHome : public BT::StatefulActionNode {
    Playback& state;
    uint64_t owner = 0;
public:
    ReturnHome(const std::string& name, const BT::NodeConfig& config, Playback& s)
        : StatefulActionNode(name, config), state(s) {}
    static BT::PortsList providedPorts() {
        return {BT::InputPort<std::string>("clip", "Cafe_Walk", "Return walking animation"),
                BT::InputPort<double>("speed", 0.792, "Return speed in world units per second")};
    }
    BT::NodeStatus onStart() override {
        if (!state.returnRequested || !state.walkEnabled) return BT::NodeStatus::FAILURE;
        owner = state.start(state.find(input<std::string>(*this,"clip")), true, true);
        state.returningHome = true; state.returnArrived = false;
        state.trackingCursor = false;
        state.walkSpeed = input<double>(*this,"speed");
        state.log("RETURN walking home");
        return BT::NodeStatus::RUNNING;
    }
    BT::NodeStatus onRunning() override {
        if (!state.returnRequested || !state.walkEnabled) { onHalted(); return BT::NodeStatus::FAILURE; }
        if (!state.returnArrived) return BT::NodeStatus::RUNNING;
        state.stop(owner); state.resetCooldown();
        state.returnRequested = state.returningHome = false;
        state.log("RETURN arrived: face screen");
        return BT::NodeStatus::SUCCESS;
    }
    void onHalted() override {
        state.stop(owner); state.resetCooldown();
        state.returnRequested = state.returningHome = false;
    }
};

inline void registerNodes(BT::BehaviorTreeFactory& factory, Playback& state) {
    factory.registerNodeType<ReturnHome>("ReturnHome", std::ref(state));
    factory.registerSimpleCondition("ActiveInputChance", [&state](BT::TreeNode& node) {
        if (!state.trackingEnabled || state.settingsOpen || !state.inputActive ||
            state.activeInputSeconds < state.activeSecondsOverride.value_or(input<double>(node, "seconds")) ||
            state.attentionAttemptEpoch == state.inputEpoch) return BT::NodeStatus::FAILURE;
        state.attentionAttemptEpoch = state.inputEpoch;
        const bool chosen = std::bernoulli_distribution(state.attentionProbabilityOverride.value_or(input<double>(node, "probability")))(state.random);
        state.log(std::string("ATTENTION active-input opportunity: ") + (chosen ? "selected" : "skipped"));
        return chosen ? BT::NodeStatus::SUCCESS : BT::NodeStatus::FAILURE;
    }, {BT::InputPort<double>("seconds", 30.0, "Continuous input duration; gaps over 3 seconds reset"),
        BT::InputPort<double>("probability", 0.5, "One chance per continuous-input session")});
    factory.registerSimpleAction("FaceCursor", [&state](BT::TreeNode&) {
        if (!state.trackingEnabled || state.settingsOpen || !state.lookTarget) return BT::NodeStatus::FAILURE;
        state.trackingCursor = true;
        state.facingRequest = state.lookYaw;
        state.log("ATTENTION face cursor: yaw=" + std::to_string(state.lookYaw));
        return BT::NodeStatus::SUCCESS;
    });
    factory.registerSimpleCondition("UserInactive", [&state](BT::TreeNode& node) {
        return state.walkEnabled && !state.settingsOpen &&
            state.inactiveSeconds >= state.idleSecondsOverride.value_or(input<double>(node, "seconds"))
            ? BT::NodeStatus::SUCCESS : BT::NodeStatus::FAILURE;
    }, {BT::InputPort<double>("seconds", 60.0, "Seconds without input in the current Windows session")});
    factory.registerNodeType<WalkRandom>("WalkRandom", std::ref(state));
    factory.registerSimpleCondition("IsDragging", [&state](BT::TreeNode&) {
        return state.dragging ? BT::NodeStatus::SUCCESS : BT::NodeStatus::FAILURE;
    });
    factory.registerSimpleCondition("HasPendingClick", [&state](BT::TreeNode&) {
        return state.pendingClick ? BT::NodeStatus::SUCCESS : BT::NodeStatus::FAILURE;
    });
    factory.registerNodeType<IdleCooldownReady>("IdleCooldownReady", std::ref(state));
    for (const std::string kind : {"PlayAnimation", "PlayRandomAnimation", "LoopAnimation"}) {
        BT::PortsList ports;
        if (kind == "PlayRandomAnimation")
            ports.insert(BT::InputPort<std::string>("clips", "Semicolon-separated model animation names"));
        else ports.insert(BT::InputPort<std::string>("clip", "Model animation name"));
        if (kind == "PlayAnimation") ports.insert(BT::InputPort<bool>("consume_click", false, "Consume one queued click at start"));
        if (kind != "LoopAnimation") ports.insert(BT::InputPort<bool>("reset_cooldown", true, "Reset idle delay after completion or interruption"));
        factory.registerBuilder(BT::TreeNodeManifest{BT::NodeType::ACTION, kind, ports, {}},
            [&state, kind](const std::string& name, const BT::NodeConfig& config) {
                return std::make_unique<AnimationAction>(name, config, state, kind);
            });
    }
}

inline void validate(const BT::Tree& tree, const Playback& state) {
    for (const auto& subtree : tree.subtrees) for (const auto& node : subtree->nodes) {
        const auto& kind = node->registrationName();
        if (kind == "ActiveInputChance") {
            const double seconds = input<double>(*node, "seconds"), probability = input<double>(*node, "probability");
            if (!std::isfinite(seconds) || seconds < 0 || !std::isfinite(probability) || probability < 0 || probability > 1)
                throw std::runtime_error("ActiveInputChance requires seconds >= 0 and probability in [0, 1]");
        }
        if (kind == "UserInactive") {
            const double seconds = input<double>(*node, "seconds");
            if (!std::isfinite(seconds) || seconds < 0) throw std::runtime_error("Inactivity threshold must be nonnegative and finite");
        }
        if (kind == "PlayAnimation" || kind == "LoopAnimation") state.find(input<std::string>(*node, "clip"));
        if (kind == "PlayRandomAnimation") clipList(state, input<std::string>(*node, "clips"));
        if (kind == "ReturnHome") {
            state.find(input<std::string>(*node, "clip"));
            const double speed = input<double>(*node, "speed");
            if (!std::isfinite(speed) || speed <= 0) throw std::runtime_error("Invalid return speed");
        }
        if (kind == "WalkRandom") {
            state.find(input<std::string>(*node, "clip"));
            const double speed = input<double>(*node, "speed");
            if (!std::isfinite(speed) || speed <= 0) throw std::runtime_error("Walk speed must be finite and positive");
            for (const char* port : {"up_weight", "up_distance", "cursor_bias"}) {
                const double value = input<double>(*node, port);
                if (!std::isfinite(value) || value < 0 || (std::string(port) != "cursor_bias" && value == 0))
                    throw std::runtime_error(std::string("Invalid walk parameter: ") + port);
            }
        }
        if (kind == "PlayAnimation") input<bool>(*node, "consume_click");
        if (kind == "PlayAnimation" || kind == "PlayRandomAnimation") input<bool>(*node, "reset_cooldown");
        if (kind == "IdleCooldownReady" || kind == "WalkRandom") {
            const double min = input<double>(*node, "min_seconds"), max = input<double>(*node, "max_seconds");
            if (!std::isfinite(min) || !std::isfinite(max) || min < 0 || max < min)
                throw std::runtime_error("Idle delay requires finite 0 <= min_seconds <= max_seconds");
            if (kind == "WalkRandom" && min <= 0) throw std::runtime_error("Walk duration must be positive");
        }
    }
}

inline std::string readText(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) throw std::runtime_error("Cannot read behavior XML: " + path.string());
    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

class Behavior {
    Playback& state;
    BT::BehaviorTreeFactory factory;
    std::unique_ptr<BT::Tree> tree;
    std::unique_ptr<BT::FileLogger2> logger;
    std::filesystem::path path, logPath;
    std::string observed, error, safeIdle;
    bool hasObserved = false;
    double pollTime = 0;
    unsigned revision = 0;
public:
    Behavior(Playback& s, std::filesystem::path xml = {}, std::filesystem::path trace = {})
        : state(s), path(std::move(xml)), logPath(std::move(trace)) { registerNodes(factory, state); }
    ~Behavior() { if (tree) tree->haltTree(); }
    std::string models() const { return BT::writeTreeNodesModelXML(factory); }
    const std::string& lastError() const { return error; }
    unsigned version() const { return revision; }
    void halt() { if (tree) tree->haltTree(); }

    bool replace(const std::string& xml) {
        try {
            auto candidate = std::make_unique<BT::Tree>(factory.createTreeFromText(xml));
            validate(*candidate, state); // Validate inactive branches too, before touching the running tree.
            if (tree) tree->haltTree();
            logger.reset();
            tree = std::move(candidate);
            state.resetCooldown();
            ++revision;
            if (!logPath.empty()) {
                auto destination = logPath;
                if (revision > 1) destination.replace_filename(logPath.stem().string() + "-" + std::to_string(revision) + ".btlog");
                try { logger = std::make_unique<BT::FileLogger2>(*tree, destination); }
                catch (const std::exception& e) { state.log(std::string("BT trace unavailable: ") + e.what()); }
            }
            error.clear();
            state.log("BT loaded revision " + std::to_string(revision));
            return true;
        } catch (const std::exception& e) {
            error = e.what();
            state.log("BT rejected; keeping previous tree: " + error);
            return false;
        }
    }
    void poll() {
        try {
            auto text = readText(path);
            if (!hasObserved || text != observed) {
                hasObserved = true;
                observed = text;
                replace(text);
                std::ofstream(path.string() + ".error.txt") << error;
            }
        } catch (const std::exception& e) {
            if (error != e.what()) { error = e.what(); state.log(error); }
        }
    }
    void start(const std::string& idleName) {
        safeIdle = idleName;
        poll();
        if (!tree) {
            // Known-safe fallback when the first XML is missing/invalid.
            auto fallback = factory.createTreeFromText("<root BTCPP_format=\"4\"><BehaviorTree ID=\"SafeIdle\">"
                "<LoopAnimation clip=\"{idle}\"/></BehaviorTree></root>");
            fallback.rootBlackboard()->set("idle", idleName);
            tree = std::make_unique<BT::Tree>(std::move(fallback));
        }
    }
    void tick(double dt, bool click = false, bool dragging = false) {
        if (state.dragging != dragging) {
            state.dragging = dragging;
            state.pendingClick = false;
            state.resetCooldown();
        }
        state.pendingClick |= click && !dragging;
        state.advance(dt);
        if (!path.empty() && (pollTime += dt) >= 0.5) { pollTime = 0; poll(); }
        if (!tree) throw std::runtime_error("Behavior has not been started");
        try {
            const auto status = tree->tickExactlyOnce();
            // Resume Idle (or a queued click) immediately after a one-shot completes.
            if (status == BT::NodeStatus::SUCCESS) tree->tickExactlyOnce();
        } catch (const std::exception& e) {
            // A syntactically valid built-in script may still fail at tick time.
            error = std::string("BT execution failed; using safe Idle: ") + e.what();
            state.log(error);
            tree->haltTree();
            logger.reset();
            auto fallback = factory.createTreeFromText("<root BTCPP_format=\"4\"><BehaviorTree ID=\"SafeIdle\">"
                "<LoopAnimation clip=\"{idle}\"/></BehaviorTree></root>");
            fallback.rootBlackboard()->set("idle", safeIdle.empty() ? state.clips.front().name : safeIdle);
            tree = std::make_unique<BT::Tree>(std::move(fallback));
            tree->tickExactlyOnce();
            if (!path.empty()) std::ofstream(path.string() + ".error.txt") << error;
        }
    }
};
} // namespace pet
