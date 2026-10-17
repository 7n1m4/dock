/*
 * This file is part of Crystal Dock.
 * Copyright (C) 2022 Viet Dang (dangvd@gmail.com)
 *
 * Crystal Dock is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Crystal Dock is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Crystal Dock.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <filesystem>
#include <iostream>
#include <csignal>
#include <cstring>
#include <cstdio>

// POSIX / glibc specific, only used by the crash handler below.
#include <execinfo.h>
#include <signal.h>
#include <sys/types.h>
#include <unistd.h>

#include <QApplication>
#include <QDir>
#include <QIcon>
#include <QSharedMemory>
#include <QStringList>

#include <model/multi_dock_model.h>
#include <view/multi_dock_view.h>

namespace {

// Prints a non-negative int to fd 2 using only write() and a local buffer.
// 'value' must be > 0; zero is handled by the caller writing "0".
void writeSignalNumber(int value) {
  char digits[24];
  int len = 0;
  do {
    digits[static_cast<size_t>(len++)] =
        static_cast<char>('0' + (value % 10));
    value /= 10;
  } while (value > 0 && len < static_cast<int>(sizeof(digits)));
  while (len > 0) {  // Write most-significant digit first.
    write(STDERR_FILENO, &digits[--len], 1);
  }
}

// Crash handler: writes a one-line report to stderr, then a backtrace, then dies
// of the signal that brought us here so that a core dump is produced and the
// exit status is the real signal status (128 + sig), not a synthesized value.
//
// This handler MUST stay async-signal-safe: raw write() and
// backtrace_symbols_fd() only. Never allocate, never lock a mutex, never call
// into Qt, never touch 'sig' after it is printed (a clobbered signal number here
// makes the re-raise a no-op and the handler returns into the fault, which used
// to loop forever instead of dying).
void crashSignalHandler(int sig) {
  static const char kPrefix[] = "\n=== crystal-dock crashed ===\n";
  static const char kLine[] = "signal = ";
  static const char kMid[] = " (crash handler running on stack)\n";
  static const char kBacktraceHeader[] = "backtrace:\n";
  static const char kBacktraceFailed[] = "backtrace: unavailable\n";
  static const char kNewline[] = "\n";

  if (sig <= 0) { sig = SIGSEGV; }

  write(STDERR_FILENO, kPrefix, sizeof(kPrefix) - 1);
  write(STDERR_FILENO, kLine, sizeof(kLine) - 1);
  writeSignalNumber(sig);
  write(STDERR_FILENO, kMid, sizeof(kMid) - 1);

#if defined(__GLIBC__)
  // backtrace_symbols_fd() formats into a static buffer without allocating, so it
  // is usable here. This is the valuable part of the handler.
  void* frames[64];
  const int count = backtrace(frames, 64);
  if (count > 0) {
    write(STDERR_FILENO, kBacktraceHeader, sizeof(kBacktraceHeader) - 1);
    backtrace_symbols_fd(frames, count, STDERR_FILENO);
  } else {
    write(STDERR_FILENO, kBacktraceFailed, sizeof(kBacktraceFailed) - 1);
  }
#else
  write(STDERR_FILENO, kBacktraceFailed, sizeof(kBacktraceFailed) - 1);
#endif

  // --- Make the process die of this signal (equivalent to it never having been
  // handled), verified empirically in this codebase: ---
  //
  //   1. SA_NODEFER in installCrashHandler(): with the default SA_NODEFER-less
  //      disposition the kernel *blocks* our signal while the handler runs, so a
  //      re-raise made here is only queued and the handler just returns -> no
  //      core, no signal exit status.
  //   2. Explicit SIG_UNBLOCK of our own signal, so it is deliverable right now.
  //   3. Restore SIG_DFL so delivery terminates the process.
  //   4. kill(getpid(), sig) to deliver it; raise()/signal() are not enough.
  {
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, sig);
    sigprocmask(SIG_UNBLOCK, &set, nullptr);

    struct sigaction action;
    memset(&action, 0, sizeof(action));
    action.sa_handler = SIG_DFL;
    sigemptyset(&action.sa_mask);
    sigaction(sig, &action, nullptr);

    kill(getpid(), sig);
  }

  // Unreachable in practice: kill() with SIG_DFL for this signal terminates.
  // If it somehow returns, exit instead of falling back into the code that just
  // faulted.
  write(STDERR_FILENO, kNewline, sizeof(kNewline) - 1);
  _exit(128 + sig);
}

// Installs the crash handler for the signals that a Qt/Wayland dock realistically
// dies from. Called before QApplication is constructed on purpose: Qt installs
// its own handlers and would otherwise win. SA_NODEFER is required, see the
// re-raise path in crashSignalHandler() above.
void installCrashHandler() {
  struct sigaction action;
  memset(&action, 0, sizeof(action));
  action.sa_handler = crashSignalHandler;
  sigemptyset(&action.sa_mask);
  action.sa_flags = SA_NODEFER;
  const int fatalSignals[] = {SIGSEGV, SIGABRT, SIGFPE};
  for (int i = 0; i < 3; ++i) {
    sigaction(fatalSignals[i], &action, nullptr);
  }
}

void maybeCopyOldConfigOnFirstRun(const QString& configDir) {
  // On the first run, copies the dock config from the old config location if it exists.
  // This is for backward compatibility.
  if (!QDir::home().exists(configDir)) {
    const auto oldConfigDir = QDir::homePath() + "/.crystal-dock-2";
    if (QDir::home().exists(oldConfigDir)) {
      std::error_code error;
      std::filesystem::copy(oldConfigDir.toStdString(), configDir.toStdString(),
          std::filesystem::copy_options::recursive, error);
      if (!error) {
        std::cout << "Copied config from " << oldConfigDir.toStdString() << " to "
                  << configDir.toStdString() << std::endl;
      } else {
        std::cerr << "Failed to copy config from " << oldConfigDir.toStdString()
                  << " to " << configDir.toStdString() << ": "
                  << error.message() << std::endl;
      }
    }
  }
}

void maybeCopyPresetConfigOnFirstRun(const QString& configDir) {
  // On the first run, copies the dock config from XDG_CONFIG_DIRS if it exists.
  // This is mainly for package managers to pre-configure the dock.
  if (!QDir::home().exists(configDir)) {
    QStringList xdgConfigDirs =
        qEnvironmentVariable("XDG_CONFIG_DIRS").split(":", Qt::SkipEmptyParts);
    for (const auto& xdgConfigDir : xdgConfigDirs) {
      const auto srcDir = xdgConfigDir + "/crystal-dock";
      if (QDir::home().exists(srcDir)) {
        std::error_code error;
        std::filesystem::copy(srcDir.toStdString(), configDir.toStdString(),
            std::filesystem::copy_options::recursive, error);
        if (!error) {
          std::cout << "Copied config from " << srcDir.toStdString() << " to "
                    << configDir.toStdString() << std::endl;
          return;
        }
        std::cerr << "Failed to copy preset config from " << srcDir.toStdString()
                  << " to " << configDir.toStdString() << ": "
                  << error.message() << std::endl;
      }
    }
  }
}

}

int main(int argc, char** argv) {
  // Must precede QApplication construction: Qt installs its own signal handlers.
  installCrashHandler();

  QApplication app(argc, argv);

  // Enforces single instance.
  QSharedMemory sharedMemory;
  sharedMemory.setKey("crystal-dock-key");
  if (!sharedMemory.create(1 /*byte*/)) {
    // The failure might have been caused by a previous crash.
    sharedMemory.attach();
    sharedMemory.detach();
    // Now try again.
    if (!sharedMemory.create(1 /*byte*/)) {
      std::cerr << "Another instance is already running." << std::endl;
      return -1;
    }
  }

  if (!crystaldock::MultiDockView::checkPlatformSupported(app)) {
    return -1;
  }

  QApplication::setWindowIcon(QIcon::fromTheme("user-desktop"));

  auto configHome = qEnvironmentVariable("XDG_CONFIG_HOME").trimmed();
  if (configHome.isEmpty()) { configHome = QDir::homePath() + "/.config"; }
  const auto configDir = configHome + "/crystal-dock";
  maybeCopyOldConfigOnFirstRun(configDir);
  maybeCopyPresetConfigOnFirstRun(configDir);
  crystaldock::MultiDockModel model(configDir);
  crystaldock::MultiDockView view(&model);

  view.show();
  return app.exec();
}
