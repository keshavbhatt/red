// name:     tv/leanback-fixes
// purpose:  the Leanback command hook and the tweaks that keep TV mode usable
//           (FEATURES T8, T15): EXIT_APP/RELOAD_PAGE go to C++,
//           the playback-cap and DRM error pop-ups are handled, mouse support
//           (enableTouchSupport) and voice search are switched on, direct
//           sign-in off, low-memory mode per setting, and a native toast API
//           for other scripts (SponsorBlock).
// depends:  window._yttv[*].instance.resolveCommand, window.tectonicConfig,
//           window.environment.feature_switches, the ?env_enableMediaStreams flag
// verified: 2026-09-06
// on-fail:  Leanback keeps its defaults (exit does nothing, no mouse)
(function () {
    'use strict';
    var red = window.__red;
    if (!red || !red.isTv()) {
        return;
    }
    var tv = red.config.tv || {};
    red.onConfig(function (c) { tv = c.tv || {}; });
    red.tv = red.tv || {};

    // --- URL flag for voice search (must be present before Leanback boots) ------
    try {
        var url = new URL(location.href);
        if (url.searchParams.get('env_enableMediaStreams') !== 'true') {
            url.searchParams.set('env_enableMediaStreams', 'true');
            history.replaceState(history.state, '', url.toString());
        }
    } catch (e) {
        red.report('tv/leanback-fixes.url', e);
    }

    // --- config switches ------------------------------------------------------------
    function patchConfigs() {
        try {
            var t = window.tectonicConfig;
            if (t && t.featureSwitches) {
                t.featureSwitches.enableTouchSupport = true;   // mouse/pointer support
                t.featureSwitches.enableDirectSignIn = false;  // the "sign in with remote" flow is broken
                t.featureSwitches.hasSamsungVoicePrivacyNotice = true; // exposes the mic setting
            }
            var env = window.environment;
            if (env && env.feature_switches) {
                env.feature_switches.enable_memory_saving_mode = !!tv.lowMemory;
            }
            var cfg = window.ytcfg && window.ytcfg.data_;
            if (cfg && cfg.INNERTUBE_CONTEXT && cfg.INNERTUBE_CONTEXT.client) {
                cfg.INNERTUBE_CONTEXT.client.webpSupport = true;
            }
        } catch (e) {
            red.report('tv/leanback-fixes.config', e);
        }
    }
    var cfgTicks = 0;
    var cfgTimer = setInterval(function () {
        patchConfigs();
        if (++cfgTicks > 120) { clearInterval(cfgTimer); }
    }, 250);

    // --- resolveCommand hook --------------------------------------------------------
    var inputModifiers = [];  // fn(cmd) -> false to swallow, cmd to replace, undefined to pass
    var resolve = null;       // the original, for red.tv.resolveCommand
    function findInstance() {
        var y = window._yttv;
        if (!y) { return null; }
        for (var key in y) {
            var inst = y[key] && y[key].instance;
            if (inst && typeof inst.resolveCommand === 'function') {
                return inst;
            }
        }
        return null;
    }
    function hook() {
        var inst = findInstance();
        if (!inst || inst.__redHooked) {
            return !!inst;
        }
        inst.__redHooked = true;
        var original = inst.resolveCommand;
        resolve = function (cmd, extra) { return original.call(inst, cmd, extra); };
        inst.resolveCommand = function (cmd, extra) {
            try {
                for (var i = 0; i < inputModifiers.length; i++) {
                    var out = inputModifiers[i](cmd);
                    if (out === false) {
                        return true;
                    }
                    if (out !== undefined) {
                        cmd = out;
                    }
                }
            } catch (e) {
                red.report('tv/leanback-fixes.command', e);
            }
            return original.call(inst, cmd, extra);
        };
        red.log('tv: resolveCommand hooked');
        return true;
    }
    var hookTimer = setInterval(function () {
        if (hook()) { clearInterval(hookTimer); }
    }, 100);

    red.tv.onCommand = function (fn) { inputModifiers.push(fn); };
    red.tv.resolveCommand = function (cmd) {
        if (resolve) {
            return resolve(cmd);
        }
        var inst = findInstance();
        return inst ? inst.resolveCommand(cmd) : false;
    };
    red.tv.toast = function (title, subtitle) {
        try {
            red.tv.resolveCommand({
                openPopupAction: {
                    popupType: 'TOAST',
                    popup: { overlayToastRenderer: {
                        title: { simpleText: String(title) },
                        subtitle: { simpleText: String(subtitle || '') }
                    } }
                }
            });
        } catch (e) {
            red.report('tv/leanback-fixes.toast', e);
        }
    };

    function signalsOf(cmd) {
        var list = [];
        if (!cmd || typeof cmd !== 'object') { return list; }
        if (cmd.signalAction && cmd.signalAction.signal) {
            list.push(cmd.signalAction.signal);
        }
        var cmds = cmd.commandExecutorCommand && cmd.commandExecutorCommand.commands;
        if (Array.isArray(cmds)) {
            cmds.forEach(function (c) { list = list.concat(signalsOf(c)); });
        }
        return list;
    }
    red.tv.onCommand(function (cmd) {
        var signals = signalsOf(cmd);
        if (signals.indexOf('EXIT_APP') >= 0) {
            red.onBridge(function (b) { b.tvSignal('exit', ''); });
            return false;
        }
        if (signals.indexOf('RELOAD_PAGE') >= 0) {
            red.onBridge(function (b) { b.tvSignal('reload', ''); });
            return false;
        }
        var popup = cmd.openPopupAction;
        if (popup && popup.uniqueId === 'playback-cap') {
            return false; // "sign in to keep watching" nag
        }
        if (popup && popup.uniqueId === 'unknown-player-error') {
            red.onBridge(function (b) { b.tvSignal('drm-error', ''); });
            red.tv.toast('This video needs DRM, which Red does not support', 'Red');
            return false;
        }
        return undefined;
    });
})();
