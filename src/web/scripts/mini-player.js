// name:     mini-player
// purpose:  page side of the mini player (FEATURES D6): when C++ shrinks the
//           window, hide everything but the player and let it fill the viewport.
// depends:  #movie_player, #masthead-container, ytd-watch-flexy structure;
//           Shorts: #shorts-player, reel-action-bar-view-model,
//           .reel-video-in-sequence-new (its contain: size layout)
// verified: 2026-09-06 against www.youtube.com (Shorts 2026-10-16)
// on-fail:  no-op (the small window shows the normal page)
(function () {
    'use strict';
    var red = window.__red;
    if (!red || red.isTv()) {
        return;
    }
    // html.red-shorts marks a Shorts page: its player is #shorts-player, and
    // the watch page's #movie_player (parked at zero size) must stay put there.
    var WATCH = 'html.red-mini:not(.red-shorts) ';
    var SHORTS = 'html.red-mini.red-shorts ';
    var CSS = [
        'html.red-mini, html.red-mini body, html.red-mini ytd-app { overflow: hidden !important; background: #000 !important; }',
        'html.red-mini #masthead-container, html.red-mini #guide, html.red-mini ytd-mini-guide-renderer,',
        'html.red-mini #secondary, html.red-mini #below, html.red-mini #comments, html.red-mini ytd-watch-next-secondary-results-renderer,',
        'html.red-mini #chat, html.red-mini ytd-playlist-panel-renderer, html.red-mini #masthead-ad',
        '{ display: none !important; }',
        WATCH + '#movie_player { overflow: hidden !important; background: #000 !important; }',
        WATCH + '#movie_player, ' + WATCH + 'ytd-player, ' + WATCH + '#player-container, ' + WATCH + '#player',
        '{ position: fixed !important; inset: 0 !important; width: 100vw !important; height: 100vh !important;',
        '  max-height: none !important; min-height: 0 !important; margin: 0 !important; padding: 0 !important; z-index: 2147483000 !important; }',
        WATCH + '#movie_player .html5-video-container { position: absolute !important; inset: 0 !important; width: 100% !important; height: 100% !important; }',
        WATCH + '#movie_player video { position: absolute !important; inset: 0 !important; width: 100% !important; height: 100% !important; object-fit: contain; }',
        WATCH + '#movie_player .ytp-chrome-bottom { width: calc(100% - 24px) !important; }',
        // Shorts: the portrait player fills the portrait window; the action
        // column, the up/down arrows and the title overlay go.
        SHORTS + 'reel-action-bar-view-model, ' + SHORTS + '#navigation-button-up, ' + SHORTS + '#navigation-button-down,',
        SHORTS + '.ytReelPlayerOverlayViewModelMetadataContainer { display: none !important; }',
        // Each Short's slot has contain: size layout, which would pin the
        // fixed player to the slot instead of the window.
        SHORTS + '.reel-video-in-sequence-new { contain: none !important; }',
        SHORTS + '#shorts-player { position: fixed !important; inset: 0 !important; width: 100vw !important; height: 100vh !important;',
        '  margin: 0 !important; border-radius: 0 !important; z-index: 2147483000 !important; }',
        SHORTS + '#shorts-player .html5-video-container { position: absolute !important; inset: 0 !important; width: 100% !important; height: 100% !important; }',
        SHORTS + '#shorts-player video { position: absolute !important; inset: 0 !important; left: 0 !important; top: 0 !important;',
        '  width: 100% !important; height: 100% !important; object-fit: contain; }'
    ].join('\n');
    var on = !!red.config.miniPlayer;
    function apply() {
        try {
            var html = document.documentElement;
            if (!html) {
                return;
            }
            red.style('red-mini-player', CSS);
            html.classList.toggle('red-shorts', /^\/shorts\//.test(location.pathname));
            html.classList.toggle('red-mini', on);
            window.dispatchEvent(new Event('resize'));
        } catch (e) {
            red.report('mini-player', e);
        }
    }
    red.setMiniPlayer = function (value) {
        on = !!value;
        apply();
    };
    red.onBridge(function (bridge) {
        bridge.miniPlayerChanged.connect(function (value) {
            on = !!value;
            apply();
        });
    });
    red.onConfig(function (c) {
        on = !!c.miniPlayer;
        apply();
    });
    // Shorts and watch pages swap without a reload (SPA navigation).
    window.addEventListener('yt-navigate-finish', function () {
        if (on) {
            apply();
        }
    });
    if (on) {
        document.addEventListener('DOMContentLoaded', apply);
    }
})();
