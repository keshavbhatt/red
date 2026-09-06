// name:     tv/identification
// purpose:  after Leanback let us in as a PS4, tell it we are a desktop Chrome
//           called "Red" (FEATURES T7) — avoids Cobalt-only
//           behaviours (licence screen, Widevine assumptions) and labels the
//           device sensibly in the account's device list.
// depends:  window.environment, window.ytcfg.data_.INNERTUBE_CONTEXT.client,
//           WEB_PLAYER_CONTEXT_CONFIGS, /youtubei/ request bodies, /tv_config
// verified: 2026-09-06
// on-fail:  no-op (Leanback keeps believing it runs on a PS4)
(function () {
    'use strict';
    var red = window.__red;
    if (!red || !red.isTv() || !red.hooks) {
        return;
    }
    var DELETE = {};
    var version = red.config.appVersion || '10';
    var chrome = (navigator.userAgent.match(/Chrome\/([\d.]+)/) || [])[1] || (/Cobalt/.test(navigator.userAgent) ? '140.0.0.0' : '140.0.0.0');
    var os = /Windows/.test(navigator.platform) ? 'Windows' : (/Mac/.test(navigator.platform) ? 'Macintosh' : 'X11');

    function merge(target, patch) {
        if (!target || typeof target !== 'object') {
            return;
        }
        for (var key in patch) {
            var value = patch[key];
            if (value === DELETE) {
                delete target[key];
            } else if (value && typeof value === 'object' && !Array.isArray(value)) {
                if (!target[key] || typeof target[key] !== 'object') {
                    target[key] = {};
                }
                merge(target[key], value);
            } else {
                target[key] = value;
            }
        }
    }
    var clientPatch = {
        platform: 'DESKTOP', platformDetail: DELETE, clientFormFactor: 'UNKNOWN_FORM_FACTOR',
        deviceMake: 'Red', deviceModel: version, browserName: 'Chrome', browserVersion: chrome,
        osName: os, osVersion: '', tvAppInfo: { releaseVehicle: DELETE }
    };
    var environmentPatch = {
        platform: 'DESKTOP', platform_detail: DELETE, brand: 'Red', model: version,
        engine: 'WebKit', browser_engine: 'WebKit', browser_engine_version: '537.36',
        browser: 'Chrome', browser_version: chrome, os: os, os_version: '',
        feature_switches: { mdx_device_label: 'Red on this computer' }
    };
    var devicePatch = {
        platform: 'DESKTOP', platformDetail: DELETE, clientFormFactor: 'UNKNOWN_FORM_FACTOR',
        deviceMake: 'Red', deviceModel: version, browserName: 'Chrome', browserVersion: chrome,
        osName: os, osVersion: '', cobaltReleaseVehicle: DELETE
    };

    function patchGlobals() {
        try {
            if (window.environment) {
                merge(window.environment, environmentPatch);
            }
            var cfg = window.ytcfg && window.ytcfg.data_;
            if (cfg) {
                if (cfg.INNERTUBE_CONTEXT && cfg.INNERTUBE_CONTEXT.client) {
                    merge(cfg.INNERTUBE_CONTEXT.client, clientPatch);
                }
                var wp = cfg.WEB_PLAYER_CONTEXT_CONFIGS;
                if (wp) {
                    for (var key in wp) {
                        if (wp[key] && wp[key].device) {
                            merge(wp[key].device, devicePatch);
                        }
                    }
                }
            }
        } catch (e) {
            red.report('tv/identification.globals', e);
        }
    }
    // The globals appear during boot; patch on a short interval until seen, then
    // keep touching them for a while (Leanback re-creates some of them).
    var ticks = 0;
    var timer = setInterval(function () {
        patchGlobals();
        if (++ticks > 120) { // 30 s
            clearInterval(timer);
        }
    }, 250);

    red.hooks.onRequest(function (url, body) {
        if (body && body.context && body.context.client) {
            merge(body.context.client, clientPatch);
        }
    });
    red.hooks.onJson(function (obj, url) {
        if (typeof url === 'string' && url.indexOf('/tv_config') >= 0 && obj) {
            if (obj.INNERTUBE_CONTEXT && obj.INNERTUBE_CONTEXT.client) {
                merge(obj.INNERTUBE_CONTEXT.client, clientPatch);
            }
            if (obj.WEB_PLAYER_CONTEXT_CONFIGS) {
                for (var key in obj.WEB_PLAYER_CONTEXT_CONFIGS) {
                    var d = obj.WEB_PLAYER_CONTEXT_CONFIGS[key] && obj.WEB_PLAYER_CONTEXT_CONFIGS[key].device;
                    if (d) { merge(d, devicePatch); }
                }
            }
        }
    });
})();
