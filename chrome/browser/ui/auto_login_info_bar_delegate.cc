// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/auto_login_info_bar_delegate.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/metrics/histogram.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/google/google_util.h"
#include "chrome/browser/infobars/infobar_tab_helper.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/ubertoken_fetcher.h"
#include "chrome/browser/tab_contents/confirm_infobar_delegate.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/browser/ui/webui/sync_promo/sync_promo_ui.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/net/gaia/gaia_constants.h"
#include "chrome/common/net/gaia/gaia_urls.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/referrer.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources_standard.h"
#include "net/base/escape.h"
#include "net/url_request/url_request.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

using content::NavigationController;

namespace {

// Enum values used for UMA histograms.
enum {
  // The infobar was shown to the user.
  HISTOGRAM_SHOWN,

  // The user pressed the accept button to perform the suggested action.
  HISTOGRAM_ACCEPTED,

  // The user pressed the reject to turn off the feature.
  HISTOGRAM_REJECTED,

  // The user pressed the X button to dismiss the infobar this time.
  HISTOGRAM_DISMISSED,

  // The user completely ignored the infoar.  Either they navigated away, or
  // they used the page as is.
  HISTOGRAM_IGNORED,

  // The user clicked on the learn more link in the infobar.
  HISTOGRAM_LEARN_MORE,

  HISTOGRAM_MAX
};

// AutoLoginRedirector --------------------------------------------------------

// This class is created by the AutoLoginInfoBarDelegate when the user wishes to
// auto-login.  It holds context information needed while re-issuing service
// tokens using the TokenService, gets the browser cookies with the TokenAuth
// API, and finally redirects the user to the correct page.
class AutoLoginRedirector : public UbertokenConsumer {
 public:
  AutoLoginRedirector(NavigationController* navigation_controller,
                      const std::string& args);
  virtual ~AutoLoginRedirector();

 private:
  // Overriden from UbertokenConsumer:
  virtual void OnUbertokenSuccess(const std::string& token) OVERRIDE;
  virtual void OnUbertokenFailure(const GoogleServiceAuthError& error) OVERRIDE;

  // Redirect tab to MergeSession URL, logging the user in and navigating
  // to the desired page.
  void RedirectToMergeSession(const std::string& token);

  NavigationController* navigation_controller_;
  const std::string args_;
  scoped_ptr<UbertokenFetcher> ubertoken_fetcher_;

  DISALLOW_COPY_AND_ASSIGN(AutoLoginRedirector);
};

AutoLoginRedirector::AutoLoginRedirector(
    NavigationController* navigation_controller,
    const std::string& args)
    : navigation_controller_(navigation_controller),
      args_(args) {
  ubertoken_fetcher_.reset(new UbertokenFetcher(
      Profile::FromBrowserContext(navigation_controller_->GetBrowserContext()),
      this));
  ubertoken_fetcher_->StartFetchingToken();
}

AutoLoginRedirector::~AutoLoginRedirector() {
}

void AutoLoginRedirector::OnUbertokenSuccess(const std::string& token) {
  RedirectToMergeSession(token);
  MessageLoop::current()->DeleteSoon(FROM_HERE, this);
}

void AutoLoginRedirector::OnUbertokenFailure(
    const GoogleServiceAuthError& error) {
  LOG(WARNING) << "AutoLoginRedirector: token request failed";
  MessageLoop::current()->DeleteSoon(FROM_HERE, this);
}

void AutoLoginRedirector::RedirectToMergeSession(const std::string& token) {
  // The args are URL encoded, so we need to decode them before use.
  std::string unescaped_args =
      net::UnescapeURLComponent(args_, net::UnescapeRule::URL_SPECIAL_CHARS);
  // TODO(rogerta): what is the correct page transition?
  navigation_controller_->LoadURL(
      GURL(GaiaUrls::GetInstance()->merge_session_url() +
          "?source=chrome&uberauth=" + token + "&" + unescaped_args),
      content::Referrer(), content::PAGE_TRANSITION_AUTO_BOOKMARK,
      std::string());
}

}  // namepsace


// AutoLoginInfoBarDelegate ---------------------------------------------------

AutoLoginInfoBarDelegate::AutoLoginInfoBarDelegate(
    InfoBarTabHelper* owner,
    const std::string& username,
    const std::string& args)
    : ConfirmInfoBarDelegate(owner),
      username_(username),
      args_(args),
      button_pressed_(false) {
  RecordHistogramAction(HISTOGRAM_SHOWN);
}

AutoLoginInfoBarDelegate::~AutoLoginInfoBarDelegate() {
  if (!button_pressed_)
    RecordHistogramAction(HISTOGRAM_IGNORED);
}

void AutoLoginInfoBarDelegate::InfoBarDismissed() {
  RecordHistogramAction(HISTOGRAM_DISMISSED);
  button_pressed_ = true;
}

gfx::Image* AutoLoginInfoBarDelegate::GetIcon() const {
  return &ResourceBundle::GetSharedInstance().GetNativeImageNamed(
      IDR_INFOBAR_AUTOLOGIN);
}

InfoBarDelegate::Type AutoLoginInfoBarDelegate::GetInfoBarType() const {
  return PAGE_ACTION_TYPE;
}

string16 AutoLoginInfoBarDelegate::GetMessageText() const {
  return l10n_util::GetStringFUTF16(IDS_AUTOLOGIN_INFOBAR_MESSAGE,
                                    UTF8ToUTF16(username_));
}

string16 AutoLoginInfoBarDelegate::GetButtonLabel(
    InfoBarButton button) const {
  return l10n_util::GetStringUTF16((button == BUTTON_OK) ?
      IDS_AUTOLOGIN_INFOBAR_OK_BUTTON : IDS_AUTOLOGIN_INFOBAR_CANCEL_BUTTON);
}

bool AutoLoginInfoBarDelegate::Accept() {
  // AutoLoginRedirector deletes itself.
  new AutoLoginRedirector(&owner()->web_contents()->GetController(), args_);
  RecordHistogramAction(HISTOGRAM_ACCEPTED);
  button_pressed_ = true;
  return true;
}

bool AutoLoginInfoBarDelegate::Cancel() {
  PrefService* pref_service =
      TabContentsWrapper::GetCurrentWrapperForContents(
          owner()->web_contents())->profile()->GetPrefs();
  pref_service->SetBoolean(prefs::kAutologinEnabled, false);
  RecordHistogramAction(HISTOGRAM_REJECTED);
  button_pressed_ = true;
  return true;
}

void AutoLoginInfoBarDelegate::RecordHistogramAction(int action) {
  UMA_HISTOGRAM_ENUMERATION("AutoLogin.Regular", action, HISTOGRAM_MAX);
}
