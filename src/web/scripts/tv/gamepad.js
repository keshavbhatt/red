// name:     tv/gamepad
// purpose:  turn Standard Gamepad input into the key events Leanback reads
//           (FEATURES T5), with repeat and stick-as-dpad. A is held like a
//           remote's OK button: keydown on press, keyup on release, so holding
//           it opens Leanback's video menu (Watch later, Save to playlist).
// depends:  navigator.getGamepads; Leanback reading keyCode on document keydown,
//           and its 500 ms Enter long press (tectonicConfig supportsLongPress)
// verified: 2026-11-04
// on-fail:  no-op (keyboard still works)
(function () {
    'use strict';
    var red = window.__red;
    if (!red || !red.isTv() || !navigator.getGamepads) {
        return;
    }
    var enabled = !!(red.config.tv && red.config.tv.gamepad);
    red.onConfig(function (c) { enabled = !!(c.tv && c.tv.gamepad); });

    // Standard Gamepad button index → keyCode (LESSONS V7 fixed).
    var BUTTONS = {
        0: 13,   // A → Enter
        1: 27,   // B → Escape (back)
        2: 170,  // X → * (search)
        3: 135,  // Y → F24 (wakes the controls)
        4: 115,  // LB → F4 (back)
        5: 116,  // RB → F5 (forward)
        6: 113,  // LT → F2 (seek back)
        7: 114,  // RT → F3 (seek forward)
        8: 189,  // Select → - (volume down)
        9: 187,  // Start → = (volume up)
        10: 77,  // L3 → M (mute)
        11: 135, // R3 → F24
        12: 38, 13: 40, 14: 37, 15: 39 // d-pad → arrows
    };
    var AXES = { 0: [37, 39], 1: [38, 40] }; // left stick x: left/right, y: up/down
    // Keys sent as one keydown on press and one keyup on release, never
    // repeated: Leanback times the gap itself (Enter: tap selects, a 500 ms
    // hold opens the video menu).
    var HOLD_KEYS = { 13: true };
    var REPEAT_DELAY = 500;
    var REPEAT_INTERVAL = 100;
    var held = {}; // key → {since, last}
    var focused = document.hasFocus();
    window.addEventListener('focus', function () { focused = true; });
    window.addEventListener('blur', function () {
        focused = false;
        for (var id in held) {
            release(id);
        }
    });

    function sendRaw(type, code) {
        var ev = new Event(type, { bubbles: true, cancelable: true });
        ev.keyCode = code;
        document.dispatchEvent(ev);
    }
    function dispatch(code) {
        if (red.tv && red.tv.sendKey) {
            red.tv.sendKey(code);
            return;
        }
        sendRaw('keydown', code);
    }
    function press(id, code, now) {
        var h = held[id];
        if (!h) {
            held[id] = { since: now, last: now, code: code };
            if (HOLD_KEYS[code]) {
                sendRaw('keydown', code);
            } else {
                dispatch(code);
            }
            return;
        }
        if (HOLD_KEYS[code]) {
            return;
        }
        if (now - h.since > REPEAT_DELAY && now - h.last > REPEAT_INTERVAL) {
            h.last = now;
            dispatch(code);
        }
    }
    function release(id) {
        var h = held[id];
        if (h && HOLD_KEYS[h.code]) {
            sendRaw('keyup', h.code);
        }
        delete held[id];
    }
    function poll() {
        requestAnimationFrame(poll);
        if (!enabled || !focused) {
            return;
        }
        var pads = navigator.getGamepads();
        var now = performance.now();
        for (var p = 0; p < pads.length; p++) {
            var pad = pads[p];
            if (!pad || !pad.connected) {
                continue;
            }
            for (var b = 0; b < pad.buttons.length; b++) {
                var code = BUTTONS[b];
                if (code === undefined) {
                    continue;
                }
                var id = 'b' + p + ':' + b;
                if (pad.buttons[b].pressed) {
                    press(id, code, now);
                } else {
                    release(id);
                }
            }
            for (var a in AXES) {
                var value = pad.axes[a] || 0;
                var neg = 'a' + p + ':' + a + '-';
                var pos = 'a' + p + ':' + a + '+';
                if (value < -0.5) { press(neg, AXES[a][0], now); } else { release(neg); }
                if (value > 0.5) { press(pos, AXES[a][1], now); } else { release(pos); }
            }
        }
    }
    window.addEventListener('gamepadconnected', function (e) {
        red.log('tv: gamepad connected: ' + (e.gamepad && e.gamepad.id));
    });
    requestAnimationFrame(poll);
})();
