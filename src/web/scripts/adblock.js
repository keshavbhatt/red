// name:     adblock
// purpose:  the response + cosmetic layers of ad blocking (ADR-003): strip ad
//           placements from player responses, ad renderers from feed/search/
//           next responses, hide residual containers and the enforcement
//           dialog, and fast-forward any ad that still starts in the player.
// depends:  __red.hooks; InnerTube field names (adPlacements, adSlots,
//           playerAds, *AdRenderer / adSlotRenderer); YouTube's #movie_player
//           element and its ad-showing class; ytd-enforcement-message-view-model;
//           mealbarPromoRenderer pop-ups (desktop and YouTube Music)
// verified: 2026-09-14 against www.youtube.com, youtube.com/tv and music.youtube.com
// on-fail:  no-op; the page keeps its own behaviour
(function () {
    'use strict';
    var red = window.__red;
    if (!red || !red.hooks) {
        return;
    }
    var enabled = !!red.config.adblock;
    red.onConfig(function (c) { enabled = !!c.adblock; applyCss(); });

    // --- response layer -------------------------------------------------------
    var PLAYER_KEYS = ['adPlacements', 'adSlots', 'playerAds', 'adBreakHeartbeatParams', 'adBreakParams'];
    var AD_RENDERERS = {
        adSlotRenderer: 1, promotedSparklesWebRenderer: 1, promotedSparklesTextSearchRenderer: 1,
        promotedVideoRenderer: 1, compactPromotedVideoRenderer: 1, displayAdRenderer: 1,
        bannerPromoRenderer: 1, statementBannerRenderer: 1, promoShelfRenderer: 1,
        adsEngagementPanelContentRenderer: 1, mealbarPromoRenderer: 1, videoMastheadAdV3Renderer: 1,
        primetimePromoRenderer: 1, brandVideoShelfRenderer: 1, brandVideoSingletonRenderer: 1,
        merchandiseShelfRenderer: 1, searchPyvRenderer: 1, tvMastheadRenderer: 1, adsEngagementPanelRenderer: 1
    };
    // Upsell pop-ups ("Try YouTube Music family plan", Premium trials) come as a
    // property of the response, not a feed entry: dropped wherever they sit.
    var PROMO_KEYS = { mealbarPromoRenderer: 1, musicMealbarPromoRenderer: 1, promotionRenderer: 1 };
    function isAdItem(el) {
        for (var key in el) {
            if (AD_RENDERERS[key]) {
                return true;
            }
        }
        var content = el.richItemRenderer && el.richItemRenderer.content;
        if (content && (content.adSlotRenderer || content.displayAdRenderer)) {
            return true;
        }
        if (el.richSectionRenderer && el.richSectionRenderer.content &&
            (el.richSectionRenderer.content.adSlotRenderer || el.richSectionRenderer.content.statementBannerRenderer)) {
            return true;
        }
        // Shelves that are pure premium upsell on TV (hideLogo metadata).
        if (el.shelfRenderer && el.shelfRenderer.tvhtml5Metadata && el.shelfRenderer.tvhtml5Metadata.hideLogo) {
            return true;
        }
        // Shorts reels that are ads.
        if (el.command && el.command.reelWatchEndpoint && el.command.reelWatchEndpoint.adClientParams &&
            el.command.reelWatchEndpoint.adClientParams.isAd) {
            return true;
        }
        return false;
    }
    function stripPlayer(obj) {
        var touched = false;
        for (var i = 0; i < PLAYER_KEYS.length; i++) {
            if (obj[PLAYER_KEYS[i]] !== undefined) {
                delete obj[PLAYER_KEYS[i]];
                touched = true;
            }
        }
        // Anti-adblock: the player response can carry a playability error with an
        // enforcement message; keep the stream but drop the message.
        if (obj.auxiliaryUi && obj.auxiliaryUi.messageRenderers &&
            obj.auxiliaryUi.messageRenderers.enforcementMessageViewModel) {
            delete obj.auxiliaryUi.messageRenderers.enforcementMessageViewModel;
            touched = true;
        }
        return touched;
    }
    red.hooks.onJson(function (obj, url) {
        if (!enabled) {
            return;
        }
        if (obj.streamingData || obj.playabilityStatus || obj.adPlacements) {
            stripPlayer(obj);
        }
        if (obj.playerResponse && typeof obj.playerResponse === 'object') {
            stripPlayer(obj.playerResponse);
        }
        // Feed shapes: anything with contents/items arrays (browse, search, next,
        // guide, reel, and the inline ytInitialData). Depth 24 covers YouTube's nesting.
        if (obj.contents || obj.onResponseReceivedActions || obj.onResponseReceivedEndpoints ||
            obj.continuationContents || obj.entries || obj.items) {
            red.hooks.prune(obj, isAdItem, 24);
        }
        red.hooks.dropKeys(obj, PROMO_KEYS, 24);
    });

    // --- cosmetic layer -------------------------------------------------------
    var CSS = [
        '#masthead-ad, #player-ads, #panels ytd-engagement-panel-section-list-renderer[target-id="engagement-panel-ads"],',
        'ytd-ad-slot-renderer, ytd-in-feed-ad-layout-renderer, ytd-banner-promo-renderer, ytd-statement-banner-renderer,',
        'ytd-promoted-sparkles-web-renderer, ytd-promoted-video-renderer, ytd-compact-promoted-video-renderer,',
        'ytd-display-ad-renderer, ytd-video-masthead-ad-v3-renderer, ytd-mealbar-promo-renderer, ytd-merch-shelf-renderer,',
        'ytd-rich-item-renderer:has(> #content > ytd-ad-slot-renderer), ytd-rich-section-renderer:has(ytd-statement-banner-renderer),',
        'ytd-action-companion-ad-renderer, ytd-brand-video-shelf-renderer, ytd-brand-video-singleton-renderer,',
        '.ytp-ad-overlay-container, .ytp-ad-image-overlay, .ytp-ad-text-overlay, .ytp-ad-action-interstitial,',
        'ytd-enforcement-message-view-model, tp-yt-paper-dialog:has(> ytd-enforcement-message-view-model),',
        'ytd-popup-container > tp-yt-paper-dialog:has(ytd-enforcement-message-view-model),',
        'ytlr-ad-slot-renderer, ytlr-promo-shelf-renderer,',
        'ytmusic-mealbar-promo-renderer, ytmusic-popup-container tp-yt-paper-dialog:has(> ytmusic-mealbar-promo-renderer),',
        'ytmusic-popup-container tp-yt-paper-dialog:has(ytmusic-mealbar-promo-renderer)',
        '{ display: none !important; }'
    ].join('\n');
    function applyCss() {
        try {
            if (enabled) {
                red.style('red-adblock', CSS);
            } else {
                red.unstyle('red-adblock');
            }
        } catch (e) {
            red.report('adblock.css', e);
        }
    }
    applyCss();

    // --- player layer: skip/fast-forward whatever still plays ------------------
    var player = null;
    var lastRate = 1;
    var forwarding = false;
    function onAdState() {
        if (!enabled || !player) {
            return;
        }
        var ad = player.classList.contains('ad-showing') || player.classList.contains('ad-interrupting');
        var video = player.querySelector('video');
        if (ad && video) {
            if (!forwarding) {
                forwarding = true;
                lastRate = video.playbackRate;
                red.log('adblock: ad detected in player, skipping');
            }
            try {
                video.muted = true;
                if (isFinite(video.duration) && video.duration > 0) {
                    video.currentTime = video.duration;
                }
                video.playbackRate = 16;
            } catch (e) { /* ignore */ }
            var skip = player.querySelector('.ytp-skip-ad-button, .ytp-ad-skip-button, .ytp-ad-skip-button-modern, .ytp-ad-skip-button-slot button');
            if (skip) {
                skip.click();
            }
            var close = player.querySelector('.ytp-ad-overlay-close-button');
            if (close) {
                close.click();
            }
        } else if (forwarding && video) {
            forwarding = false;
            try {
                video.muted = false;
                video.playbackRate = lastRate || 1;
            } catch (e) { /* ignore */ }
        }
    }
    function dismissEnforcement() {
        if (!enabled) {
            return;
        }
        var dialog = document.querySelector('ytd-enforcement-message-view-model');
        if (!dialog) {
            return;
        }
        var popup = dialog.closest('tp-yt-paper-dialog');
        if (popup) {
            popup.remove();
        } else {
            dialog.remove();
        }
        var backdrop = document.querySelector('tp-yt-iron-overlay-backdrop');
        if (backdrop) {
            backdrop.remove();
        }
        var video = document.querySelector('#movie_player video');
        if (video && video.paused) {
            video.play().catch(function () {});
        }
        red.log('adblock: enforcement dialog dismissed');
    }
    var playerObserver = new MutationObserver(onAdState);
    function attachPlayer() {
        var p = document.getElementById('movie_player') || document.querySelector('.html5-video-player');
        if (p === player) {
            return;
        }
        playerObserver.disconnect();
        player = p;
        if (player) {
            playerObserver.observe(player, { attributes: true, attributeFilter: ['class'] });
            onAdState();
        }
    }
    // The popup container is where the enforcement dialog and the player land;
    // watching body children (not subtree) keeps this observer cheap.
    var bodyObserver = new MutationObserver(function () {
        attachPlayer();
        dismissEnforcement();
    });
    document.addEventListener('DOMContentLoaded', function () {
        attachPlayer();
        bodyObserver.observe(document.body, { childList: true });
        var popups = document.querySelector('ytd-popup-container');
        if (popups) {
            new MutationObserver(dismissEnforcement).observe(popups, { childList: true, subtree: true });
        }
    });
    window.addEventListener('yt-navigate-finish', function () {
        attachPlayer();
        dismissEnforcement();
    });
})();
