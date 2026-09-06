// name:     mini-player
// purpose:  page side of the mini player (FEATURES D6): when C++ shrinks the
//           window, hide everything but the player and let it fill the viewport.
// depends:  #movie_player, #masthead-container, ytd-watch-flexy structure
// verified: 2026-09-06 against www.youtube.com
// on-fail:  no-op (the small window shows the normal page)
(function () {
    'use strict';
    var red = window.__red;
    if (!red || red.isTv()) {
        return;
    }
    var CSS = [
        'html.red-mini, html.red-mini body { overflow: hidden !important; background: #000 !important; }',
        'html.red-mini #masthead-container, html.red-mini #guide, html.red-mini ytd-mini-guide-renderer,',
        'html.red-mini #secondary, html.red-mini #below, html.red-mini #comments, html.red-mini ytd-watch-next-secondary-results-renderer,',
        'html.red-mini #chat, html.red-mini ytd-playlist-panel-renderer, html.red-mini #masthead-ad',
        '{ display: none !important; }',
        'html.red-mini #movie_player, html.red-mini ytd-player, html.red-mini #player-container, html.red-mini #player',
        '{ position: fixed !important; inset: 0 !important; width: 100vw !important; height: 100vh !important;',
        '  max-height: none !important; min-height: 0 !important; margin: 0 !important; padding: 0 !important; z-index: 2147483000 !important; }',
        'html.red-mini #movie_player video { width: 100% !important; height: 100% !important; left: 0 !important; top: 0 !important; object-fit: contain; }',
        'html.red-mini #movie_player .ytp-chrome-bottom { width: calc(100% - 24px) !important; }'
    ].join('\n');
    var on = !!red.config.miniPlayer;
    function apply() {
        try {
            var html = document.documentElement;
            if (!html) {
                return;
            }
            red.style('red-mini-player', CSS);
            html.classList.toggle('red-mini', on);
            window.dispatchEvent(new Event('resize'));
        } catch (e) {
            red.report('mini-player', e);
        }
    }
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
    if (on) {
        document.addEventListener('DOMContentLoaded', apply);
    }
})();
