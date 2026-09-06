// name:     tv/h5vcc
// purpose:  Cobalt's host API shim (FEATURES T6, LESSONS V1): Leanback
//           expects window.h5vcc for deep links, DIAL and the video container
//           size (which caps the playback resolution).
// depends:  Leanback reading window.h5vcc.{runtime,system,dial}
// verified: 2026-09-06
// on-fail:  Leanback falls back to its non-Cobalt paths
(function () {
    'use strict';
    var red = window.__red;
    if (!red || !red.isTv() || window.h5vcc) {
        return;
    }
    var tv = red.config.tv || {};
    red.onConfig(function (c) { tv = c.tv || {}; });
    var SIZES = ['256x144', '426x240', '640x360', '854x480', '1280x720', '1920x1080', '2560x1440', '3840x2160', '7680x4320'];
    function containerSize() {
        if (tv.unlockResolution) {
            return '7680x4320';
        }
        var w = (window.screen && window.screen.width) || 1920;
        var h = (window.screen && window.screen.height) || 1080;
        for (var i = 0; i < SIZES.length; i++) {
            var parts = SIZES[i].split('x');
            if (parseInt(parts[0], 10) >= w && parseInt(parts[1], 10) >= h) {
                return SIZES[i];
            }
        }
        return SIZES[SIZES.length - 1];
    }
    var deepLinkListeners = [];
    function DialServer() {}
    DialServer.prototype.onRequest = function () {};
    DialServer.prototype.listen = function () {};
    try {
        window.h5vcc = {
            runtime: {
                initialDeepLink: '',
                onDeepLink: {
                    addListener: function (fn) { deepLinkListeners.push(fn); },
                    removeListener: function (fn) {
                        var i = deepLinkListeners.indexOf(fn);
                        if (i >= 0) { deepLinkListeners.splice(i, 1); }
                    }
                }
            },
            system: {
                getVideoContainerSizeOverride: containerSize
            },
            dial: { DialServer: DialServer },
            accessibility: {}
        };
        red.tv = red.tv || {};
        red.tv.deepLink = function (url) {
            deepLinkListeners.forEach(function (fn) {
                try { fn(url); } catch (e) { red.report('tv/h5vcc.deepLink', e); }
            });
        };
    } catch (e) {
        red.report('tv/h5vcc', e);
    }
})();
