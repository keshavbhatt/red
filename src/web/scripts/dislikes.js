// name:     dislikes
// purpose:  show the Return YouTube Dislike count under the dislike button
//           (FEATURES D14). The count comes from C++ over the bridge.
// depends:  __red.bridge (requestDislikes / dislikes); yt-navigate-finish;
//           dislike-button-view-model (the segmented like/dislike control)
// verified: 2026-09-06 against www.youtube.com
// on-fail:  no-op (the button stays as YouTube renders it)
(function () {
    'use strict';
    var red = window.__red;
    if (!red || red.isTv()) {
        return;
    }
    var enabled = !!red.config.dislikes;
    var currentId = '';
    var currentCount = -1;
    red.onConfig(function (c) {
        enabled = !!c.dislikes;
        if (!enabled) {
            remove();
        } else if (currentId) {
            request(currentId);
        }
    });

    function videoIdFromLocation() {
        try {
            var url = new URL(location.href);
            if (url.pathname === '/watch') {
                return url.searchParams.get('v') || '';
            }
        } catch (e) { /* ignore */ }
        return '';
    }
    function request(id) {
        red.onBridge(function (bridge) { bridge.requestDislikes(id); });
    }
    function format(n) {
        if (n < 1000) { return String(Math.round(n)); }
        if (n < 1000000) { return (n / 1000).toFixed(n < 10000 ? 1 : 0).replace(/\.0$/, '') + 'K'; }
        return (n / 1000000).toFixed(n < 10000000 ? 1 : 0).replace(/\.0$/, '') + 'M';
    }
    function button() {
        // The segmented control: like on the left, dislike on the right. Several
        // instances exist (templates, the mini player); take the rendered one.
        var buttons = document.querySelectorAll('dislike-button-view-model button');
        for (var i = 0; i < buttons.length; i++) {
            var r = buttons[i].getBoundingClientRect();
            if (r.width > 0 && r.height > 0) {
                return buttons[i];
            }
        }
        return null;
    }
    function remove() {
        var el = document.querySelector('.red-dislike-count');
        if (el) {
            el.remove();
        }
    }
    function render() {
        if (!enabled || currentCount < 0) {
            return;
        }
        var b = button();
        if (!b) {
            return;
        }
        var el = b.querySelector('.red-dislike-count');
        if (!el) {
            el = document.createElement('span');
            el.className = 'red-dislike-count';
            b.appendChild(el);
            b.classList.add('red-has-dislikes');
            // The dislike button is an icon-only pill; widen it like the like button.
            red.style('red-dislikes', 'button.red-has-dislikes { width: auto !important; padding: 0 16px 0 12px !important; }' +
                '.red-dislike-count { margin-left: 6px; font-size: 1.4rem; font-weight: 500; line-height: 2rem; white-space: nowrap; }');
        }
        el.textContent = format(currentCount);
        b.setAttribute('aria-label', 'Dislike this video along with ' + format(currentCount) + ' other people');
    }
    function onNavigate() {
        var id = videoIdFromLocation();
        if (id !== currentId) {
            currentId = id;
            currentCount = -1;
            remove();
        }
        if (id && enabled) {
            request(id);
        }
    }
    red.onBridge(function (bridge) {
        bridge.dislikes.connect(function (videoId, count) {
            if (videoId !== currentId) {
                return;
            }
            currentCount = count;
            render();
            // The button re-renders a few times after navigation; keep it in place.
            var tries = 0;
            var timer = setInterval(function () {
                render();
                if (++tries > 10 || document.querySelector('.red-dislike-count')) {
                    clearInterval(timer);
                }
            }, 700);
        });
    });
    window.addEventListener('yt-navigate-finish', onNavigate);
    document.addEventListener('DOMContentLoaded', onNavigate);
})();
