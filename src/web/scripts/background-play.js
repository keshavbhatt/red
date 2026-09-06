// name:     background-play
// purpose:  keep playback going when the window is hidden or minimized
//           (FEATURES D7, LESSONS V8): YouTube pauses on visibilitychange.
// depends:  document.visibilityState / hidden, the visibilitychange event
// verified: 2026-09-06
// on-fail:  no-op (YouTube pauses when hidden)
(function () {
    'use strict';
    var red = window.__red;
    if (!red) {
        return;
    }
    var enabled = !!red.config.backgroundPlay;
    red.onConfig(function (c) { enabled = !!c.backgroundPlay; });
    try {
        var stop = function (e) {
            if (enabled) {
                e.stopImmediatePropagation();
            }
        };
        document.addEventListener('visibilitychange', stop, true);
        document.addEventListener('webkitvisibilitychange', stop, true);
        var hiddenDesc = Object.getOwnPropertyDescriptor(Document.prototype, 'hidden');
        var stateDesc = Object.getOwnPropertyDescriptor(Document.prototype, 'visibilityState');
        if (hiddenDesc && hiddenDesc.get && stateDesc && stateDesc.get) {
            Object.defineProperty(document, 'hidden', {
                configurable: true,
                get: function () { return enabled ? false : hiddenDesc.get.call(document); }
            });
            Object.defineProperty(document, 'visibilityState', {
                configurable: true,
                get: function () { return enabled ? 'visible' : stateDesc.get.call(document); }
            });
        }
    } catch (e) {
        red.report('background-play', e);
    }
})();
