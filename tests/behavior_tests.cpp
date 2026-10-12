#include "pet_behavior.h"
#include <iostream>
#include <cstring>

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
int main(int argc, char** argv) {
    try {
        require(argc == 3, "Usage: behavior_tests reference.xml active.xml");
        pet::Playback state;
        state.random.seed(1);
        state.clips = {{"Cafe_Idle", 2, 0}, {"Cafe_Reaction", 1, 1}, {"Cafe_Walk", 1, 2},
                       {"Formation_Pickup", 1, 3}, {"Exs", 1, 4}};
        pet::Behavior behavior(state);
        {
            pet::Playback attention;
            attention.lookTarget = true;
            attention.lookYaw = 135;
            attention.cursorYaw = 45; // Walking keeps a separate ground-level target.
            pet::Behavior attentionTree(attention);
            const std::string lookTree = "<root BTCPP_format=\"4\"><BehaviorTree ID=\"Attention\"><Sequence>"
                "<ActiveInputChance seconds=\"30\" probability=\"1\"/><FaceCursor/>"
                "</Sequence></BehaviorTree></root>";
            require(attentionTree.replace(lookTree), "Attention tree must load");
            require(!attention.observeInput(0, 0), "Startup is not recovery from inactivity");
            for (int i = 0; i < 29; ++i) attention.observeInput(0, 1);
            attentionTree.tick(0);
            require(!attention.facingRequest, "Attention must not run before 30 seconds of sustained input");
            attention.observeInput(0, 1); attentionTree.tick(0);
            require(attention.facingRequest && *attention.facingRequest == 135, "Attention must face cursor after threshold");
            require(attention.current == -1 && !attention.active, "Attention preview must not start an unselected animation");
            attention.facingRequest.reset();
            attention.observeInput(0, 30); attentionTree.tick(0);
            require(!attention.facingRequest && attention.trackingCursor, "One chance must latch ongoing tracking without retriggering");
            attention.observeInput(4, 0);
            require(attention.activeInputSeconds == 0, "Input gap over three seconds must reset continuity");
            require(!attention.observeInput(0, 0), "Short input gap must not force facing screen");
            require(attention.trackingCursor, "Short pauses must preserve tracking");
            attention.observeInput(60, 0);
            require(!attention.trackingCursor, "Entering no-input state must stop tracking");
            require(attention.observeInput(0, 0), "Recovery after no-input state must face screen");
            require(!attention.observeInput(0, 1), "Further input must not repeatedly reset facing");
            auto noChance = lookTree;
            noChance.replace(noChance.find("probability=\"1\""), 15, "probability=\"0\"");
            require(attentionTree.replace(noChance), "Zero probability must be supported");
            attention.observeInput(0, 30); attentionTree.tick(0);
            require(!attention.facingRequest, "Failed probability check must leave orientation unchanged");
            require(attentionTree.replace(lookTree), "Restore test probability");
            attentionTree.tick(0);
            require(!attention.facingRequest, "Reload must not reroll consumed opportunity");
            attention.observeInput(4, 0); attention.observeInput(0, 0); attention.observeInput(0, 30);
            attentionTree.tick(0);
            require(attention.facingRequest.has_value(), "A new input session must allow a new opportunity");
        }
        {
            pet::Playback actual;
            actual.clips = state.clips;
            pet::Behavior editedTree(actual);
            require(editedTree.replace(pet::readText(argv[2])), "Actual edited tree must load");
            editedTree.tick(0);
            editedTree.tick(0, true);
            require(actual.active && !actual.loop, "Actual tree click must play a one-shot");
            editedTree.tick(0, false, true);
            require(actual.active && actual.loop && !actual.walking, "Actual tree drag must interrupt click");
            const auto draggingOwner = actual.generation;
            editedTree.tick(20, false, true);
            require(actual.generation == draggingOwner, "Actual tree must stay in dragging branch");
            editedTree.tick(0, false, false);
            require(actual.active && actual.loop && actual.generation > draggingOwner,
                    "Actual tree release/cancel must interrupt drag branch");
            editedTree.tick(20);
            require(actual.active && actual.loop, "Actual tree must not get stuck behind an empty condition branch");
            actual.inactiveSeconds = 59.9;
            editedTree.tick(60);
            require(!actual.walking, "Actual tree must not wander before 60 seconds without user input");
            actual.inactiveSeconds = 60;
            editedTree.tick(0);
            require(actual.walking, "Actual tree must allow walking after 60 seconds without input");
            actual.walkEnabled = false;
            editedTree.tick(0);
            require(!actual.walking, "Settings toggle must immediately halt an ongoing walk");
            actual.walkEnabled = true;
            actual.idleSecondsOverride = 5;
            actual.inactiveSeconds = 5;
            editedTree.tick(20);
            require(actual.walking, "Settings idle threshold must override XML default");
            actual.settingsOpen = true;
            editedTree.tick(0);
            require(!actual.walking, "Opening settings must pause autonomous walking");
            actual.settingsOpen = false;
            actual.inactiveSeconds = 0;
            editedTree.tick(0.01);
            require(!actual.walking && actual.loop && actual.active, "Any new input must halt an ongoing autonomous walk");
            actual.returnRequested=true;
            editedTree.tick(0);
            require(actual.returningHome && actual.walking && actual.current==2,
                    "Return must use walking animation despite active user input");
            actual.returnArrived=true;
            editedTree.tick(0);
            require(!actual.returningHome && !actual.returnRequested && !actual.walking,
                    "Arrival must finish return and restore idle");
            actual.returnRequested=true;
            editedTree.tick(0);
            editedTree.tick(0,false,true);
            require(!actual.returningHome && !actual.returnRequested && actual.current==3,
                    "Dragging must interrupt returning home");
        }
        {
            pet::Playback configured;
            configured.lookTarget = true;
            configured.activeSecondsOverride = 5;
            configured.observeInput(0, 0);
            configured.observeInput(0, 5);
            pet::Behavior tree(configured);
            require(tree.replace("<root BTCPP_format=\"4\"><BehaviorTree ID=\"Settings\"><Sequence>"
                "<ActiveInputChance seconds=\"30\" probability=\"1\"/><FaceCursor/>"
                "</Sequence></BehaviorTree></root>"), "Configured attention tree must load");
            configured.trackingEnabled = false;
            tree.tick(0);
            require(!configured.trackingCursor, "Disabled tracking must block automatic activation");
            configured.trackingEnabled = true;
            configured.settingsOpen = true;
            tree.tick(0);
            require(!configured.trackingCursor, "Settings menu must pause automatic activation");
            configured.settingsOpen = false;
            tree.tick(0);
            require(configured.trackingCursor, "Settings continued-input threshold must override XML default");
            configured.idleSecondsOverride = 12;
            configured.observeInput(12, 0);
            require(!configured.trackingCursor && configured.observeInput(0, 0),
                    "Configured inactivity threshold must also control input recovery");
        }
        {
            pet::Playback walker;
            walker.clips = state.clips;
            walker.canWalk = [](double yaw, double) { return yaw == 180; };
            pet::Behavior walkTree(walker);
            require(walkTree.replace("<root BTCPP_format=\"4\"><BehaviorTree ID=\"Walk\"><ReactiveFallback>"
                "<ReactiveSequence><IsDragging/><LoopAnimation clip=\"Cafe_Idle\"/></ReactiveSequence>"
                "<Sequence><HasPendingClick/><PlayAnimation clip=\"Cafe_Reaction\" consume_click=\"true\"/></Sequence>"
                "<Sequence><IdleCooldownReady min_seconds=\"1\" max_seconds=\"1\"/>"
                "<WalkRandom min_seconds=\"3\" max_seconds=\"3\" up_distance=\"1\"/></Sequence>"
                "<LoopAnimation clip=\"Cafe_Idle\"/></ReactiveFallback></BehaviorTree></root>"), "Walk tree must load");
            walkTree.tick(0); walkTree.tick(1.1);
            require(walker.walking && walker.walkYaw == 180 && walker.current == 2, "Walk must choose an in-bounds heading and Cafe_Walk");
            walkTree.tick(1.5);
            require(walker.walking && walker.time < 1 && walker.idleTime == 1.1, "Walk animation must loop without counting as Idle");
            walkTree.tick(0, true);
            require(!walker.walking && walker.current == 1 && walker.active, "Click must cancel locomotion without halting new reaction");
            walkTree.tick(1.1); walkTree.tick(1.1);
            require(walker.walking, "Walk must resume after fresh Idle delay");
            walkTree.tick(0, false, true);
            require(!walker.walking && walker.loop && walker.active, "Drag must stop locomotion");
            walkTree.tick(0); walkTree.tick(1.1); walkTree.tick(3.1);
            require(!walker.walking && walker.loop && walker.current == 0, "Completed walk must return to Idle");
        }
        {
            pet::Playback sampled;
            sampled.clips = state.clips;
            sampled.random.seed(42);
            BT::BehaviorTreeFactory factory;
            pet::registerNodes(factory, sampled);
            auto tree = factory.createTreeFromText("<root BTCPP_format=\"4\"><BehaviorTree ID=\"Sampling\">"
                "<WalkRandom min_seconds=\"2\" max_seconds=\"2\"/></BehaviorTree></root>");
            int upward = 0, towardBefore = 0, towardAfter = 0;
            for (int pass = 0; pass < 2; ++pass) {
                sampled.cursorTarget = pass == 1; sampled.cursorYaw = 90;
                for (int i = 0; i < 4000; ++i) {
                    tree.tickExactlyOnce();
                    const bool up = sampled.walkYaw > 90 && sampled.walkYaw < 270;
                    if (!pass && up) ++upward;
                    require(std::abs(sampled.walkRemaining - (up ? 5 : 2)) < 0.001, "Upward steps must be 2.5 times longer");
                    if (sampled.walkYaw == 90) { if (pass) ++towardAfter; else ++towardBefore; }
                    tree.haltTree();
                }
            }
            require(upward > 600 && upward < 1000, "Upward choice probability must be reduced to roughly 19 percent");
            require(towardAfter > towardBefore*1.4, "Cursor bias must increase movement toward cursor");
            sampled.movementScale = 1.6;
            double checkedDistance = 0;
            sampled.canWalk = [&](double, double distance) { checkedDistance = distance; return true; };
            tree.tickExactlyOnce();
            require(checkedDistance >= 0.792*2*1.6 - 0.001, "Bounds prediction must include manual model zoom");
            tree.haltTree();
        }
        auto xml = pet::readText(argv[1]);
        require(behavior.replace(xml), "Default XML must load");
        behavior.tick(0);
        require(state.loop && state.current == 0, "Must begin in Idle");
        behavior.tick(4.9);
        require(state.loop, "Random activity must wait at least five idle seconds");
        behavior.tick(7.2);
        require(!state.loop && state.current >= 2 && state.active, "Random branch must start by twelve idle seconds");
        const auto randomOwner = state.generation;
        behavior.tick(0.1, true);
        require(state.current == 1 && state.active && !state.pendingClick, "Click must preempt random without old halt stopping new clip");
        require(state.generation == randomOwner + 1, "Click starts exactly once");
        const auto clickOwner = state.generation;
        for (int i = 0; i < 5; ++i) behavior.tick(0.1, true);
        require(state.generation == clickOwner && state.pendingClick, "Repeated clicks queue one event, never restart current reaction");
        behavior.tick(0.6);
        require(state.current == 1 && state.generation == clickOwner + 1 && !state.pendingClick, "Queued click runs after previous reaction");
        behavior.tick(1.1);
        require(state.loop && state.current == 0 && state.idleTime == 0, "Reaction finishes into Idle with fresh cooldown");
        behavior.tick(4.9);
        require(state.loop, "Action duration must not count as idle cooldown");
        behavior.tick(0, true);
        behavior.tick(0.1, true);
        require(!state.loop && state.pendingClick, "Set up an active reaction and queued click");
        behavior.tick(0, false, true);
        require(state.active && state.loop && state.current == 0 && !state.pendingClick,
                "Drag must interrupt reaction into Idle and discard queued click");
        const auto dragOwner = state.generation;
        behavior.tick(30, false, true);
        require(state.generation == dragOwner && state.idleTime == 0, "Holding drag must not trigger random activity");
        behavior.tick(0, false, false);
        require(state.active && state.loop && state.generation > dragOwner,
                "Release/capture cancellation must halt drag branch and resume normal Idle");
        behavior.tick(4.9);
        require(state.loop, "Release must restart the idle cooldown");
        const auto revision = behavior.version();
        const auto owner = state.generation;
        require(!behavior.replace("<root>broken"), "Malformed XML must be rejected");
        require(state.generation == owner && state.active && behavior.version() == revision, "Invalid reload preserves running state");
        auto badClip = xml;
        badClip.replace(badClip.find("Cafe_Reaction"), std::string("Cafe_Reaction").size(), "MissingClip");
        require(!behavior.replace(badClip), "Unknown clip in inactive branch must be rejected");
        auto badDelay = xml;
        badDelay.replace(badDelay.find("min_seconds=\"5\""), 15, "min_seconds=\"20\"");
        require(!behavior.replace(badDelay), "Inverted delay range must be rejected");
        auto edited = xml;
        edited.replace(edited.find("min_seconds=\"5\""), 15, "min_seconds=\"0\"");
        edited.replace(edited.find("max_seconds=\"12\""), 16, "max_seconds=\"0\"");
        require(behavior.replace(edited), "Valid edited XML must reload");
        behavior.tick(0);
        require(!state.loop && state.current >= 2 && state.active, "Edited zero cooldown must affect execution");
        require(behavior.models().find("PlayRandomAnimation") != std::string::npos, "Generated Groot model must include registered nodes");
        const auto work = std::filesystem::current_path()/"behavior-test-output";
        std::filesystem::create_directories(work);
        const auto file = work/"hotreload.xml";
        { std::ofstream(file) << ""; }
        {
            pet::Playback live;
            live.clips = state.clips;
            pet::Behavior watched(live, file, work/"trace.btlog");
            watched.start("Cafe_Idle");
            watched.tick(0);
            require(live.loop && !watched.lastError().empty(), "Empty startup file must report error and use safe Idle");
            { std::ofstream(file) << xml; }
            watched.tick(0.5);
            require(watched.version() == 1 && watched.lastError().empty(), "Saving valid XML must recover from startup fallback");
            { std::ofstream(file) << badClip; }
            watched.tick(0.5);
            require(watched.version() == 1 && !watched.lastError().empty() && live.active, "Bad file save must retain last good tree");
            { std::ofstream(file) << edited; }
            watched.tick(0.5);
            require(watched.version() == 2 && !live.loop, "File watcher must apply edited delay");
            const std::string script = "<root BTCPP_format=\"4\"><BehaviorTree ID=\"BadScript\"><Script code=\"missing += 1\"/></BehaviorTree></root>";
            require(watched.replace(script), "Runtime-error test script must parse");
            watched.tick(0);
            require(live.loop && watched.lastError().find("execution failed") != std::string::npos,
                    "Runtime error must recover to safe Idle instead of closing pet");
        }
        for (const auto& name : {"trace.btlog", "trace-2.btlog"}) {
            auto bytes = pet::readText(work/name);
            require(bytes.starts_with("BTCPP4-FileLogger2"), "Trace must have official Groot2 format");
            uint32_t xmlSize = 0;
            constexpr size_t headerBytes = std::string_view("BTCPP4-FileLogger2").size() + 1;
            std::memcpy(&xmlSize, bytes.data() + headerBytes, 4);
            require(bytes.size() > headerBytes + 4 + xmlSize + 8, "Trace must contain node transitions beyond XML header");
        }
        std::cout << "PASS: idle, delay, priority, interruption, click queue, completion, invalid reload, edited XML, node models, file watcher, safe fallback, Groot logs\n";
        return 0;
    } catch (const std::exception& e) { std::cerr << "FAIL: " << e.what() << '\n'; return 1; }
}
