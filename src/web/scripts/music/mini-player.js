// name:     music/mini-player
// purpose:  page side of the mini player in Music mode: C++ shrinks the window
//           to a portrait size and YouTube Music's own responsive player page
//           (art, title, seek bar, controls, Up next) fills it. Opens the player
//           page on entry and closes it again on exit if it was closed before.
// depends:  ytmusic-app-layout[player-ui-state] (PLAYER_PAGE_OPEN), the visible
//           ytmusic-miniplayer[slot=player-bar] (its title opens the page),
//           ytmusic-player-page #collapse-button
// verified: 2026-10-16 against music.youtube.com
// on-fail:  no-op (the small window shows whatever page YouTube Music is on)
(function () {
    'use strict';
    var red = window.__red;
    if (!red || !red.isMusic || !red.isMusic()) {
        return;
    }
    // The collapse chevron would drop the tiny window onto the browse pages.
    var CSS = 'html.red-mini ytmusic-player-page #collapse-button { visibility: hidden !important; }';
    var OPEN = 'PLAYER_PAGE_OPEN';
    var on = !!red.config.miniPlayer;
    var openedByUs = false;
    var tries = 0;
    var timer = 0;

    function layout() {
        return document.querySelector('ytmusic-app-layout');
    }
    function isOpen() {
        var l = layout();
        return !!l && l.getAttribute('player-ui-state') === OPEN;
    }
    // The bar exists twice (one copy parked at zero size): use the visible one.
    function visibleBar() {
        var bars = document.querySelectorAll('ytmusic-miniplayer');
        for (var i = 0; i < bars.length; i++) {
            if (bars[i].getBoundingClientRect().width > 0) {
                return bars[i];
            }
        }
        return null;
    }
    function openPage() {
        clearTimeout(timer);
        if (!on || isOpen()) {
            return;
        }
        var bar = visibleBar();
        var title = bar && bar.querySelector('.ytmusicTrackInfoTitle');
        if (title) {
            title.click();
            openedByUs = true;
        }
        // The layout reflows with the window: check again until it is open.
        if (++tries < 12) {
            timer = setTimeout(openPage, 300);
        }
    }
    function closePage() {
        clearTimeout(timer);
        if (!openedByUs || !isOpen()) {
            openedByUs = false;
            return;
        }
        openedByUs = false;
        var collapse = document.querySelector('ytmusic-player-page #collapse-button button');
        if (collapse) {
            collapse.click();
        }
    }
    function apply() {
        try {
            var html = document.documentElement;
            if (!html) {
                return;
            }
            red.style('red-music-mini-player', CSS);
            html.classList.toggle('red-mini', on);
            if (on) {
                tries = 0;
                openPage();
            } else {
                closePage();
            }
        } catch (e) {
            red.report('music/mini-player', e);
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
