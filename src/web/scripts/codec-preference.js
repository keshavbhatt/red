// name:     codec-preference
// purpose:  steer YouTube's codec choice (FEATURES D4, h264ify-style): hide
//           the codecs the user does not want from MediaSource/canPlayType so
//           the player picks the next best. H.264 costs a fraction of the CPU
//           of VP9/AV1 software decode.
// depends:  MediaSource.isTypeSupported, HTMLMediaElement.prototype.canPlayType
// verified: 2026-09-06
// on-fail:  no-op (YouTube picks its own codec)
(function () {
    'use strict';
    var red = window.__red;
    if (!red) {
        return;
    }
    var codec = red.config.codec || 'auto';
    function blocked(type) {
        if (codec === 'auto' || typeof type !== 'string') {
            return false;
        }
        var t = type.toLowerCase();
        if (codec === 'h264') {
            return t.indexOf('vp9') >= 0 || t.indexOf('vp09') >= 0 || t.indexOf('av01') >= 0 || t.indexOf('webm') >= 0 || t.indexOf('vp8') >= 0;
        }
        if (codec === 'vp9') {
            return t.indexOf('av01') >= 0;
        }
        return false;
    }
    try {
        if (window.MediaSource && MediaSource.isTypeSupported) {
            var nativeIsType = MediaSource.isTypeSupported.bind(MediaSource);
            MediaSource.isTypeSupported = function (type) {
                return blocked(type) ? false : nativeIsType(type);
            };
        }
        if (window.HTMLMediaElement) {
            var nativeCanPlay = HTMLMediaElement.prototype.canPlayType;
            HTMLMediaElement.prototype.canPlayType = function (type) {
                return blocked(type) ? '' : nativeCanPlay.call(this, type);
            };
        }
    } catch (e) {
        red.report('codec-preference', e);
    }
    // A live change applies to the next video (the player probes codecs at load).
    red.onConfig(function (c) { codec = c.codec || 'auto'; });
})();
