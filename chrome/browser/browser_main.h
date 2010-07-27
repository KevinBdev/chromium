// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSER_MAIN_H_
#define CHROME_BROWSER_BROWSER_MAIN_H_
#pragma once

#include "base/basictypes.h"
#include "base/field_trial.h"
#include "base/scoped_ptr.h"
#include "base/tracked_objects.h"

class ChromeThread;
class CommandLine;
class HighResolutionTimerManager;
struct MainFunctionParams;
class MessageLoop;
class MetricsService;
class SystemMonitor;

namespace net {
class NetworkChangeNotifier;
}

// BrowserMainParts:
// This class contains different "stages" to be executed in |BrowserMain()|,
// mostly initialization. This is made into a class rather than just functions
// so each stage can create and maintain state. Each part is represented by a
// single method (e.g., "EarlyInitialization()"), which does the following:
//  - calls a method (e.g., "PreEarlyInitialization()") which individual
//    platforms can override to provide platform-specific code which is to be
//    executed before the common code;
//  - calls various methods for things common to all platforms (for that given
//    stage); and
//  - calls a method (e.g., "PostEarlyInitialization()") for platform-specific
//    code to be called after the common code.
// As indicated above, platforms should override the default "Pre...()" and
// "Post...()" methods when necessary; they need not call the superclass's
// implementation (which is empty).
//
// Parts:
//  - EarlyInitialization: things which should be done as soon as possible on
//    program start (such as setting up signal handlers) and things to be done
//    at some generic time before the start of the main message loop.
//  - MainMessageLoopStart: things beginning with the start of the main message
//    loop and ending with initialization of the main thread; platform-specific
//    things which should be done immediately before the start of the main
//    message loop should go in |PreMainMessageLoopStart()|.
//  - (more to come)
class BrowserMainParts {
 public:
  // This static method is to be implemented by each platform and should
  // instantiate the appropriate subclass.
  static BrowserMainParts* CreateBrowserMainParts(
      const MainFunctionParams& parameters);

  virtual ~BrowserMainParts();

  // Parts to be called by |BrowserMain()|.
  void EarlyInitialization();
  void MainMessageLoopStart();

 protected:
  explicit BrowserMainParts(const MainFunctionParams& parameters);

  // Accessors for data members (below) ----------------------------------------
  const MainFunctionParams& parameters() const {
    return parameters_;
  }
  const CommandLine& parsed_command_line() const {
    return parsed_command_line_;
  }
  MessageLoop& main_message_loop() const {
    return *main_message_loop_;
  }

  // Methods to be overridden to provide platform-specific code; these
  // correspond to the "parts" above.
  virtual void PreEarlyInitialization() {}
  virtual void PostEarlyInitialization() {}
  virtual void PreMainMessageLoopStart() {}
  virtual void PostMainMessageLoopStart() {}

 private:
  // Methods for |EarlyInitialization()| ---------------------------------------

  // A/B test for the maximum number of persistent connections per host.
  void ConnectionFieldTrial();

  // A/B test for determining a value for unused socket timeout.
  void SocketTimeoutFieldTrial();

  // A/B test for spdy when --use-spdy not set.
  void SpdyFieldTrial();

  // Used to initialize NSPR where appropriate.
  void InitializeSSL();

  // Methods for |MainMessageLoopStart()| --------------------------------------

  void InitializeMainThread();

  // Members initialized on construction ---------------------------------------

  const MainFunctionParams& parameters_;
  const CommandLine& parsed_command_line_;

#if defined(TRACK_ALL_TASK_OBJECTS)
  // Creating this object starts tracking the creation and deletion of Task
  // instance. This MUST be done before main_message_loop, so that it is
  // destroyed after the main_message_loop.
  tracked_objects::AutoTracking tracking_objects_;
#endif

  // Statistical testing infrastructure for the entire browser.
  FieldTrialList field_trial_;

  // Members initialized in |MainMessageLoopStart()| ---------------------------
  scoped_ptr<MessageLoop> main_message_loop_;
  scoped_ptr<SystemMonitor> system_monitor_;
  scoped_ptr<HighResolutionTimerManager> hi_res_timer_manager_;
  scoped_ptr<net::NetworkChangeNotifier> network_change_notifier_;
  scoped_ptr<ChromeThread> main_thread_;

  DISALLOW_COPY_AND_ASSIGN(BrowserMainParts);
};


// Perform platform-specific work that needs to be done after the main event
// loop has ended.
void DidEndMainMessageLoop();

// Records the conditions that can prevent Breakpad from generating and
// sending crash reports.  The presence of a Breakpad handler (after
// attempting to initialize crash reporting) and the presence of a debugger
// are registered with the UMA metrics service.
void RecordBreakpadStatusUMA(MetricsService* metrics);

// Displays a warning message if some minimum level of OS support is not
// present on the current platform.
void WarnAboutMinimumSystemRequirements();

#endif  // CHROME_BROWSER_BROWSER_MAIN_H_
