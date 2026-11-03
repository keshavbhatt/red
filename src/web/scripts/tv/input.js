// name:     tv/input
// purpose:  mouse & keyboard conveniences for Leanback (FEATURES T4):
//           right click = back, wheel swallowed, cursor hidden when idle,
//           Ctrl+D downloads the current video, +/- volume, M mute, C captions,
//           D/A speed (Shift for larger steps), all only when the page has focus.
// depends:  Leanback reading keyCode on document keydown events; the
//           .html5-video-player API (setVolume/getVolume/setPlaybackRate)
// verified: 2026-09-06
// on-fail:  no-op
(function () {
    'use strict';
    var red = window.__red;
    if (!red || !red.isTv()) {
        return;
    }
    function sendKey(code) {
        var down = new Event('keydown', { bubbles: true, cancelable: true });
        down.keyCode = code;
        document.dispatchEvent(down);
        setTimeout(function () {
            var up = new Event('keyup', { bubbles: true, cancelable: true });
            up.keyCode = code;
            document.dispatchEvent(up);
        }, 50);
    }
    red.tv = red.tv || {};
    red.tv.sendKey = sendKey;

    // Speed keys: D / A step by 0.05 (issue #16: 1.1x, 1.15x), with Shift by 0.25, within
    // Leanback's own range. Through the player API, so Leanback's state stays in step.
    var SPEED_STEP = 0.05;
    var SPEED_STEP_LARGE = 0.25;
    var SPEED_MIN = 0.25;
    var SPEED_MAX = 2;
    function stepSpeed(p, delta) {
        if (!p.setPlaybackRate || !p.getPlaybackRate) {
            return;
        }
        var rate = Math.round((p.getPlaybackRate() + delta) * 100) / 100;
        rate = Math.min(SPEED_MAX, Math.max(SPEED_MIN, rate));
        p.setPlaybackRate(rate);
        indicator(p.getPlaybackRate() + '×');
    }

    function currentVideoUrl() {
        try {
            var route = location.hash.charAt(0) === '#' ? location.hash.substring(1) : location.hash;
            var u = new URL(route, location.href);
            var id = u.pathname === '/watch' ? u.searchParams.get('v') : '';
            return id ? 'https://www.youtube.com/watch?v=' + id : '';
        } catch (e) {
            return '';
        }
    }
    function player() {
        return document.querySelector('.html5-video-player');
    }
    red.style('red-tv-indicator-style', '#red-tv-indicator{position:fixed;top:24px;right:32px;background:rgba(28,28,28,.92);color:#f1f1f1;font:500 22px Roboto,Arial,sans-serif;padding:10px 18px;border-radius:8px;z-index:2147483646;pointer-events:none;transition:opacity .2s;opacity:0}#red-tv-indicator.red-on{opacity:1}');
    function indicator(text) {
        var el = document.getElementById('red-tv-indicator');
        if (!el) {
            el = document.createElement('div');
            el.id = 'red-tv-indicator';
            document.body.appendChild(el);
        }
        el.textContent = text;
        el.classList.add('red-on');
        clearTimeout(indicator.timer);
        indicator.timer = setTimeout(function () { el.classList.remove('red-on'); }, 1200);
    }

    // Right click → back; wheel swallowed (Leanback scrolls with keys).
    document.addEventListener('mousedown', function (e) {
        if (e.button === 2) {
            e.preventDefault();
            e.stopImmediatePropagation();
            sendKey(27);
        }
    }, true);
    document.addEventListener('contextmenu', function (e) { e.preventDefault(); }, true);
    document.addEventListener('wheel', function (e) { e.preventDefault(); }, { capture: true, passive: false });

    // Cursor auto-hide.
    var cursorTimer = null;
    function showCursor() {
        if (!document.documentElement) {
            return;
        }
        document.documentElement.style.cursor = '';
        clearTimeout(cursorTimer);
        cursorTimer = setTimeout(function () {
            if (document.documentElement) {
                document.documentElement.style.cursor = 'none';
            }
        }, 3000);
    }
    document.addEventListener('mousemove', showCursor, true);
    document.addEventListener('DOMContentLoaded', showCursor);

    // Hotkeys.
    document.addEventListener('keydown', function (e) {
        var p = player();
        var ctrl = e.ctrlKey || e.metaKey;
        if (ctrl && (e.key === 'd' || e.key === 'D')) {
            var url = currentVideoUrl();
            if (url) {
                red.onBridge(function (b) { b.downloadRequested(url); });
            }
            e.preventDefault();
            e.stopImmediatePropagation();
            return;
        }
        if (ctrl && e.shiftKey && (e.key === 'c' || e.key === 'C')) {
            var link = currentVideoUrl();
            if (link && navigator.clipboard) {
                navigator.clipboard.writeText(link).then(function () { indicator('Link copied'); });
            }
            e.preventDefault();
            e.stopImmediatePropagation();
            return;
        }
        if (!p || ctrl || e.altKey) {
            return;
        }
        var handled = true;
        switch (e.key) {
        case '+': case '=':
            if (p.setVolume && p.getVolume) { p.setVolume(Math.min(100, p.getVolume() + 5)); indicator('Volume ' + p.getVolume() + '%'); }
            break;
        case '-': case '_':
            if (p.setVolume && p.getVolume) { p.setVolume(Math.max(0, p.getVolume() - 5)); indicator('Volume ' + p.getVolume() + '%'); }
            break;
        case 'm': case 'M':
            if (p.isMuted && p.mute && p.unMute) { if (p.isMuted()) { p.unMute(); indicator('Unmuted'); } else { p.mute(); indicator('Muted'); } }
            break;
        case 'd': case 'D':
            stepSpeed(p, e.shiftKey ? SPEED_STEP_LARGE : SPEED_STEP);
            break;
        case 'a': case 'A':
            stepSpeed(p, -(e.shiftKey ? SPEED_STEP_LARGE : SPEED_STEP));
            break;
        case 's': case 'S':
            if (p.setPlaybackRate) { p.setPlaybackRate(1); indicator('1×'); }
            break;
        default:
            handled = false;
        }
        if (handled) {
            e.preventDefault();
            e.stopImmediatePropagation();
        }
    }, true);
})();
