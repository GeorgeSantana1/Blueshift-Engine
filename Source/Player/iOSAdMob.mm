// Copyright(c) 2017 POLYGONTEK
// 
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
// 
// http ://www.apache.org/licenses/LICENSE-2.0
// 
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "Precompiled.h"
#include "Application.h"
#include "iOSAdMob.h"

UIViewController<GADBannerViewDelegate, GADInterstitialDelegate, GADRewardBasedVideoAdDelegate> *AdMob::viewController;

AdMob::BannerAd AdMob::bannerAd;
AdMob::InterstitialAd AdMob::interstitialAd;
AdMob::RewardBasedVideoAd AdMob::rewardBasedVideoAd;

BE1::StrArray AdMob::testDeviceList;

void AdMob::RegisterLuaModule(LuaCpp::State *state, UIViewController<GADBannerViewDelegate, GADInterstitialDelegate, GADRewardBasedVideoAdDelegate> *viewController) {
    AdMob::viewController = viewController;

    state->RegisterModule("admob", [](LuaCpp::Module &module) {
        module["init"].SetFunc(AdMob::Init);

        LuaCpp::Selector _BannerAd = module["BannerAd"];

        _BannerAd.SetObj(bannerAd);
        _BannerAd.AddObjMembers(bannerAd,
            "init", &BannerAd::Init,
            "request", &BannerAd::Request,
            "show", &BannerAd::Show,
            "hide", &BannerAd::Hide);

        LuaCpp::Selector _InterstitialAd = module["InterstitialAd"];
        
        _InterstitialAd.SetObj(interstitialAd);
        _InterstitialAd.AddObjMembers(interstitialAd,
            "init", &InterstitialAd::Init,
            "request", &InterstitialAd::Request,
            "is_ready", &InterstitialAd::IsReady,
            "present", &InterstitialAd::Present);

        LuaCpp::Selector _RewardBasedVideoAd = module["RewardBasedVideoAd"];
        
        _RewardBasedVideoAd.SetObj(rewardBasedVideoAd);
        _RewardBasedVideoAd.AddObjMembers(rewardBasedVideoAd,
            "init", &RewardBasedVideoAd::Init,
            "request", &RewardBasedVideoAd::Request,
            "is_ready", &RewardBasedVideoAd::IsReady,
            "present", &RewardBasedVideoAd::Present);
    });
}

void AdMob::Init(const char *appID, const char *testDevices) {
    // Initialize Google mobile ads
    NSString *nsAppID = [[NSString alloc] initWithBytes:appID length:strlen(appID) encoding:NSUTF8StringEncoding];
    [GADMobileAds configureWithApplicationID:nsAppID];

    BE1::StrArray testDeviceList;
    BE1::SplitStringIntoList(testDeviceList, testDevices, " ");
}

//-------------------------------------------------------------------------------------------------

void AdMob::BannerAd::Init() {
}

void AdMob::BannerAd::Request(const char *unitID, int adWidth, int adHeight) {
    GADRequest *request = [GADRequest request];

    if (testDeviceList.Count() > 0) {
        NSString *nsTestDevices[256];
        for (int i = 0; i < testDeviceList.Count(); i++) {
            nsTestDevices[i] = [[NSString alloc] initWithBytes:testDeviceList[i].c_str() length:testDeviceList[i].Length() encoding:NSUTF8StringEncoding];
        }
    
        request.testDevices = [NSArray arrayWithObjects:nsTestDevices count:testDeviceList.Count()];
    }
    
    // ad unit of interstitial provided by Google for testing purposes.
    static const char *testUnitID = "ca-app-pub-3940256099942544/2934735716";
#ifdef _DEBUG
    unitID = testUnitID;
#else
    if (!unitID || !unitID[0]) {
        unitID = testUnitID;
    }
#endif

    NSString *nsUnitID = [[NSString alloc] initWithBytes:unitID length:strlen(unitID) encoding:NSUTF8StringEncoding];

    // NOTE: adWidth <= 0 means full screen width
    GADAdSize size = GADAdSizeFromCGSize(CGSizeMake(adWidth > 0 ? adWidth : AdMob::viewController.view.bounds.size.width, adHeight));
    bannerView = [[GADBannerView alloc] initWithAdSize:size];
    bannerView.translatesAutoresizingMaskIntoConstraints = NO;
    bannerView.adUnitID = nsUnitID;

    // This view controller is used to present an overlay when the ad is clicked. 
    // It should normally be set to the view controller that contains the GADBannerView.
    bannerView.rootViewController = AdMob::viewController;

    bannerView.delegate = AdMob::viewController;

    [bannerView loadRequest:request];
}

void AdMob::BannerAd::Show(bool showOnBottomOfScreen, float offsetX, float offsetY) {
    if (bannerView.superview) {
        return;
    }

    UIView *view = AdMob::viewController.view;

    [view addSubview:bannerView];
    
    int x = view.bounds.size.width * offsetX + 0.5f;
    int y = view.bounds.size.height * offsetY + 0.5f;
    
    if (@available(ios 11.0, *)) {
        UILayoutGuide *guide = view.safeAreaLayoutGuide;
        
        NSMutableArray *constraints = [NSMutableArray array];
        [constraints addObject:[bannerView.centerXAnchor constraintEqualToAnchor:guide.centerXAnchor constant:x]];
        
        if (showOnBottomOfScreen) {
            [constraints addObject:[bannerView.bottomAnchor constraintEqualToAnchor:guide.bottomAnchor constant:-y]];
        } else {
            [constraints addObject:[bannerView.topAnchor constraintEqualToAnchor:guide.topAnchor constant:y]];
        }
        
        [NSLayoutConstraint activateConstraints:constraints];
    } else {
        NSMutableArray *constraints = [NSMutableArray array];
        [constraints addObject:[NSLayoutConstraint constraintWithItem:bannerView
                                                            attribute:NSLayoutAttributeCenterX
                                                            relatedBy:NSLayoutRelationEqual
                                                               toItem:view
                                                            attribute:NSLayoutAttributeCenterX
                                                           multiplier:1
                                                             constant:x]];
        
        if (showOnBottomOfScreen) {
            [constraints addObject:[NSLayoutConstraint constraintWithItem:bannerView
                                                                attribute:NSLayoutAttributeBottom
                                                                relatedBy:NSLayoutRelationEqual
                                                                   toItem:AdMob::viewController.bottomLayoutGuide
                                                                attribute:NSLayoutAttributeTop
                                                               multiplier:1
                                                                 constant:-y]];
        } else {
            [constraints addObject:[NSLayoutConstraint constraintWithItem:bannerView
                                                                attribute:NSLayoutAttributeTop
                                                                relatedBy:NSLayoutRelationEqual
                                                                   toItem:AdMob::viewController.topLayoutGuide
                                                                attribute:NSLayoutAttributeTop
                                                               multiplier:1
                                                                 constant:y]];
        }
        
        [view addConstraints:constraints];
    }
}

void AdMob::BannerAd::Hide() {
    if (bannerView.superview) {
        [bannerView removeFromSuperview];
    }
}

//-------------------------------------------------------------------------------------------------

void AdMob::InterstitialAd::Init() {
}

void AdMob::InterstitialAd::Request(const char *unitID) {
    GADRequest *request = [GADRequest request];

    if (testDeviceList.Count() > 0) {
        NSString *nsTestDevices[256];
        for (int i = 0; i < testDeviceList.Count(); i++) {
            nsTestDevices[i] = [[NSString alloc] initWithBytes:testDeviceList[i].c_str() length:testDeviceList[i].Length() encoding:NSUTF8StringEncoding];
        }
    
        request.testDevices = [NSArray arrayWithObjects:nsTestDevices count:testDeviceList.Count()];
    }
    
    // ad unit of interstitial provided by Google for testing purposes.
    static const char *testUnitID = "ca-app-pub-3940256099942544/4411468910";
#ifdef _DEBUG
    unitID = testUnitID;
#else
    if (!unitID || !unitID[0]) {
        unitID = testUnitID;
    }
#endif

    NSString *nsUnitID = [[NSString alloc] initWithBytes:unitID length:strlen(unitID) encoding:NSUTF8StringEncoding];

    interstitial = [[GADInterstitial alloc] initWithAdUnitID:nsUnitID];
    interstitial.delegate = AdMob::viewController;

    [interstitial loadRequest:request];
}

bool AdMob::InterstitialAd::IsReady() const {
    return [interstitial isReady];
}

void AdMob::InterstitialAd::Present() {
    [interstitial presentFromRootViewController:AdMob::viewController];
}

//-------------------------------------------------------------------------------------------------

void AdMob::RewardBasedVideoAd::Init() {
}

void AdMob::RewardBasedVideoAd::Request(const char *unitID) {
    GADRequest *request = [GADRequest request];

    if (testDeviceList.Count() > 0) {
        NSString *nsTestDevices[256];
        for (int i = 0; i < testDeviceList.Count(); i++) {
            nsTestDevices[i] = [[NSString alloc] initWithBytes:testDeviceList[i].c_str() length:testDeviceList[i].Length() encoding:NSUTF8StringEncoding];
        }
    
        request.testDevices = [NSArray arrayWithObjects:nsTestDevices count:testDeviceList.Count()];
    }
    
    // ad unit of rewarded video provided by Google for testing purposes.
    static const char *testUnitID = "ca-app-pub-3940256099942544/1712485313";
#ifdef _DEBUG
    unitID = testUnitID;
#else
    if (!unitID || !unitID[0]) {
        unitID = testUnitID;
    }
#endif

    NSString *nsUnitID = [[NSString alloc] initWithBytes:unitID length:strlen(unitID) encoding:NSUTF8StringEncoding];

    // Required to set the delegate prior to loading an ad.
    [GADRewardBasedVideoAd sharedInstance].delegate = AdMob::viewController;

    // Request to load an ad.
    [[GADRewardBasedVideoAd sharedInstance] loadRequest:request withAdUnitID:nsUnitID];
}

bool AdMob::RewardBasedVideoAd::IsReady() const {
    return [[GADRewardBasedVideoAd sharedInstance] isReady];
}

void AdMob::RewardBasedVideoAd::Present() {
    [[GADRewardBasedVideoAd sharedInstance] presentFromRootViewController:AdMob::viewController];
}
