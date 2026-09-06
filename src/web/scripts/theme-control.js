// name:     theme-control
// purpose:  make the page follow Red's theme setting (FEATURES D1). YouTube
//           keys its dark styling on the `dark` attribute of <html>; forcing
//           that attribute is enough and survives navigation (YouTube only
//           re-derives it when its own theme menu is used).
// depends:  html[dark]; matchMedia('(prefers-color-scheme: dark)') for "system"
// verified: 2026-09-06 against www.youtube.com
// on-fail:  no-op (YouTube's own theme applies)
(function () {
    'use strict';
    var red = window.__red;
    if (!red || red.isTv()) {
        return;
    }
    var scheme = red.config.colorScheme || 'system';
    var media = window.matchMedia ? window.matchMedia('(prefers-color-scheme: dark)') : null;
    var applying = false;

    function wantDark() {
        if (scheme === 'dark') { return true; }
        if (scheme === 'light') { return false; }
        return !!(media && media.matches);
    }
    function apply() {
        try {
            var html = document.documentElement;
            if (!html) {
                return;
            }
            applying = true;
            var dark = wantDark();
            if (dark && !html.hasAttribute('dark')) {
                html.setAttribute('dark', '');
            } else if (!dark && html.hasAttribute('dark')) {
                html.removeAttribute('dark');
            }
            html.style.colorScheme = dark ? 'dark' : 'light';
            applying = false;
        } catch (e) {
            applying = false;
            red.report('theme-control', e);
        }
    }
    // YouTube (re)sets the attribute during boot; keep ours in place for the
    // explicit choices, leave "system" to the page.
    var observer = new MutationObserver(function () {
        if (!applying && scheme !== 'system') {
            apply();
        }
    });
    function observe() {
        if (document.documentElement) {
            observer.observe(document.documentElement, { attributes: true, attributeFilter: ['dark'] });
        }
    }
    red.onConfig(function (c) {
        scheme = c.colorScheme || 'system';
        apply();
    });
    if (media && media.addEventListener) {
        media.addEventListener('change', function () { if (scheme === 'system') { apply(); } });
    }
    apply();
    observe();
    document.addEventListener('DOMContentLoaded', apply);
    window.addEventListener('yt-navigate-finish', apply);
})();
