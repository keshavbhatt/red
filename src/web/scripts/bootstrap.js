// name:     bootstrap
// purpose:  create window.__red (config + error reporting + config updates)
//           and connect the QWebChannel bridge; every other script relies on it.
// depends:  qt.webChannelTransport (QWebChannel), nothing from YouTube
// verified: 2026-09-06
// on-fail:  no-op; reports are queued until the bridge is ready
//
// The C++ side prepends `window.__redConfig = {...};` and qwebchannel.js to
// this file, so `QWebChannel` and the config exist when it runs.
(function () {
    'use strict';
    if (window.__red) {
        return;
    }
    // Only YouTube (and Red's own about:blank error page) get the hooks.
    // Google's sign-in pages in particular inspect the environment; patched
    // natives there look like an "insecure browser".
    var host = location.hostname;
    if (host && !/(^|\.)(youtube|youtube-nocookie|googlevideo)\.com$/.test(host)) {
        return;
    }
    var pending = [];
    var bridgeWaiters = [];
    var configWaiters = [];
    var api = {
        config: window.__redConfig || {},
        bridge: null,
        report: function (name, error) {
            var message = error && error.message ? error.message : String(error);
            if (api.bridge) {
                api.bridge.scriptFailed(name, message);
            } else {
                pending.push([name, message]);
            }
        },
        log: function (message) {
            if (api.bridge) {
                api.bridge.log(String(message));
            }
        },
        // Runs fn(bridge) now if connected, else once the channel is up.
        onBridge: function (fn) {
            if (api.bridge) {
                try { fn(api.bridge); } catch (e) { api.report('onBridge', e); }
            } else {
                bridgeWaiters.push(fn);
            }
        },
        // Runs fn(config) on every live config change (and never for the initial one).
        onConfig: function (fn) {
            configWaiters.push(fn);
        },
        mode: function () {
            return api.config.mode || 'desktop';
        },
        isTv: function () {
            return api.config.mode === 'tv';
        },
        isMusic: function () {
            return api.config.mode === 'music';
        },
        // Inserts (or replaces) a <style id> with the page's CSP nonce copied
        // onto it; the nonce only exists once the document has parsed, so the
        // element is re-created at DOMContentLoaded when it was added earlier.
        style: function (id, css) {
            var apply = function () {
                var root = document.head || document.documentElement;
                if (!root) {
                    return;
                }
                var existing = document.getElementById(id);
                var nonced = document.querySelector('style[nonce], script[nonce], link[nonce]');
                var nonce = nonced && nonced.nonce ? nonced.nonce : '';
                if (existing && existing.getAttribute('data-red-nonce') === (nonce ? '1' : '0') && existing.textContent === css) {
                    return;
                }
                if (existing) {
                    existing.remove();
                }
                var el = document.createElement('style');
                el.id = id;
                if (nonce) {
                    el.nonce = nonce;
                }
                el.setAttribute('data-red-nonce', nonce ? '1' : '0');
                el.textContent = css;
                root.appendChild(el);
            };
            apply();
            if (document.readyState === 'loading') {
                document.addEventListener('DOMContentLoaded', apply, { once: true });
            }
        },
        unstyle: function (id) {
            var el = document.getElementById(id);
            if (el) {
                el.remove();
            }
        }
    };
    window.__red = api;
    try {
        if (typeof QWebChannel === 'function' && window.qt && window.qt.webChannelTransport) {
            new QWebChannel(window.qt.webChannelTransport, function (channel) {
                api.bridge = channel.objects.bridge || null;
                if (!api.bridge) {
                    return;
                }
                pending.forEach(function (p) { api.bridge.scriptFailed(p[0], p[1]); });
                pending = [];
                try {
                    api.bridge.configChanged.connect(function (config) {
                        api.config = config || api.config;
                        configWaiters.forEach(function (fn) {
                            try { fn(api.config); } catch (e) { api.report('onConfig', e); }
                        });
                    });
                } catch (e) {
                    api.report('bootstrap.configChanged', e);
                }
                var waiters = bridgeWaiters;
                bridgeWaiters = [];
                waiters.forEach(function (fn) {
                    try { fn(api.bridge); } catch (e) { api.report('onBridge', e); }
                });
            });
        }
    } catch (e) {
        pending.push(['bootstrap', e && e.message ? e.message : String(e)]);
    }
})();
