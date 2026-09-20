#include "backend.hpp"
#include <cerrno>
#include <chrono>
#include <cstring>
#include <fcntl.h>
#include <filesystem>
#include <signal.h>
#include <stdexcept>
#include <sys/prctl.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>
namespace sapfui {
std::string findLauncher() {
    if (auto p = std::getenv("SAPF_ENGINE"))
        return p;
    if (auto home = std::getenv("HOME")) {
        std::string candidate = std::string(home) + "/src/sapf-linux/run-sapf";
        if (access(candidate.c_str(), X_OK) == 0)
            return candidate;
    }
    return "sapf";
}
Backend::Backend(std::string launcher) : launcher_(std::move(launcher)) { signal(SIGPIPE, SIG_IGN); }
Backend::~Backend() { closeProcess(); }
void Backend::append(const std::string &s) {
    log += s;
    if (log.size() > 65536)
        log.erase(0, log.size() - 49152);
}
bool Backend::start() {
    if (pid_ > 0)
        return true;
    int in[2], out[2];
    if (pipe2(in, O_CLOEXEC) != 0) {
        error = strerror(errno);
        return false;
    }
    if (pipe2(out, O_CLOEXEC) != 0) {
        close(in[0]);
        close(in[1]);
        error = strerror(errno);
        return false;
    }
    const pid_t parent = getpid();
    pid_ = fork();
    if (pid_ == 0) {
        prctl(PR_SET_PDEATHSIG, SIGTERM);
        if (getppid() != parent)
            _exit(1);
        dup2(in[0], STDIN_FILENO);
        dup2(out[1], STDOUT_FILENO);
        dup2(out[1], STDERR_FILENO);
        close(in[0]);
        close(in[1]);
        close(out[0]);
        close(out[1]);
        execlp(launcher_.c_str(), launcher_.c_str(), (char *)nullptr);
        dprintf(STDERR_FILENO, "Cannot launch SAPF: %s\n", strerror(errno));
        _exit(127);
    }
    close(in[0]);
    close(out[1]);
    if (pid_ < 0) {
        close(in[1]);
        close(out[0]);
        error = strerror(errno);
        return false;
    }
    input_ = in[1];
    output_ = out[0];
    fcntl(input_, F_SETFL, O_NONBLOCK);
    fcntl(output_, F_SETFL, O_NONBLOCK);
    error.clear();
    append("\n[SAPF session started]\n");
    return true;
}
void Backend::closeProcess() {
    if (input_ >= 0)
        close(input_);
    if (output_ >= 0)
        close(output_);
    input_ = output_ = -1;
    if (pid_ > 0) {
        kill(pid_, SIGTERM);
        bool done = false;
        for (int i = 0; i < 20; ++i) {
            if (waitpid(pid_, nullptr, WNOHANG) != 0) {
                done = true;
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        if (!done) {
            kill(pid_, SIGKILL);
            waitpid(pid_, nullptr, 0);
        }
    }
    pid_ = -1;
    playing = false;
    root = 0;
    pending_.clear();
    awaiting_.clear();
    values_.clear();
    scoped_ = false;
}
void Backend::send(const std::string &text) {
    if (!start())
        return;
    if (pending_.size() + text.size() > 262144) {
        error = "Interpreter input queue is full; use Reset engine.";
        return;
    }
    pending_ += text + "\n";
}
void Backend::poll() {
    if (pid_ <= 0)
        return;
    if (!pending_.empty()) {
        ssize_t n = write(input_, pending_.data(), pending_.size());
        if (n > 0)
            pending_.erase(0, n);
        else if (n < 0 && errno != EAGAIN && errno != EINTR) {
            error = "SAPF input closed.";
            closeProcess();
            return;
        }
    }
    char buf[8192];
    std::string received;
    size_t total = 0;
    while (total < 65536) {
        ssize_t n = read(output_, buf, sizeof(buf));
        if (n <= 0)
            break;
        total += size_t(n);
        received.append(buf, size_t(n));
        append(std::string(buf, size_t(n)));
    }
    if (received.find("error:") != std::string::npos || received.find("parse error") != std::string::npos ||
        received.find("exception in real time") != std::string::npos) {
        error = "SAPF reported an error. Open SAPF console for details.";
        if (!awaiting_.empty() || received.find("exception in real time") != std::string::npos) {
            playing = false;
            root = 0;
            awaiting_.clear();
        }
    }
    if (!awaiting_.empty() && log.find(awaiting_) != std::string::npos) {
        playing = true;
        awaiting_.clear();
    }
    if (waitpid(pid_, nullptr, WNOHANG) == pid_) {
        pid_ = -1;
        error = "SAPF exited. Set SAPF_ENGINE to a Linux SAPF launcher, then Reset engine.";
        closeProcess();
        append("\n[Interpreter exited]\n");
    }
}
static std::string topology(Graph g, int root) {
    for (auto &[id, n] : g.nodes)
        if (n.op == "number")
            n.value = 0;
    return g.code(root, true);
}
void Backend::play(const Graph &g, int id) {
    g.validate();
    if (!start())
        return;
    error.clear();
    // Scope bindings are discarded between patches rather than accumulating forever.
    std::string code = "stop\n";
    if (scoped_)
        code += "popWorkspace\n";
    code += "pushWorkspace\n";
    scoped_ = true;
    values_.clear();
    scalar_ = g.scalarNumbers(id);
    for (int k : g.reachable(id)) {
        auto &n = g.nodes.at(k);
        if (n.op == "number") {
            values_[k] = n.value;
            code += number(n.value) + " ZR = gui" + std::to_string(k) + "\n";
        }
    }
    // The user's patch supplies its own gain. Clip extreme output as a final guard.
    code += g.code(id, true) + " 0.9 clip2 play ";
    awaiting_ = "GUI_PLAYING_" + std::to_string(++serial_);
    code += '"' + awaiting_ + "\" pr cr\n";
    root = id;
    playing = false;
    topology_ = topology(g, id);
    send(code);
    append("\n> " + g.code(id) + " play\n");
}
void Backend::sync(const Graph &g) {
    if (!root || (!playing && awaiting_.empty()))
        return;
    if (!g.nodes.count(root) || std::find(g.roots.begin(), g.roots.end(), root) == g.roots.end()) {
        stop();
        return;
    }
    bool restart = topology(g, root) != topology_;
    for (auto &[id, value] : values_) {
        auto it = g.nodes.find(id);
        if (it == g.nodes.end()) {
            restart = true;
            break;
        }
        if (it->second.value != value && scalar_.count(id))
            restart = true;
    }
    if (restart) {
        play(g, root);
        return;
    }
    std::string changes;
    for (auto &[id, value] : values_) {
        double next = g.nodes.at(id).value;
        if (next != value) {
            changes += number(next) + " gui" + std::to_string(id) + " set\n";
            value = next;
        }
    }
    if (!changes.empty())
        send(changes);
}
void Backend::stop() {
    if (pid_ > 0)
        send("stop");
    playing = false;
    awaiting_.clear();
    root = 0;
}
void Backend::reset() {
    closeProcess();
    start();
}
} // namespace sapfui
