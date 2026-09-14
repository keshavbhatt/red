// name:     hooks
// purpose:  one registry of response modifiers shared by adblock / hide-shorts /
//           TV identification: JSON.parse results, fetch Response bodies, XHR
//           bodies, and the inline ytInitialPlayerResponse / ytInitialData.
//           Installing the hooks once at DocumentCreation, before any page
//           script runs, removes the need to hold early requests (LESSONS V3).
// depends:  standard JSON / fetch / XMLHttpRequest; YouTube globals by name only
// verified: 2026-09-06
// on-fail:  the original functions keep working; a throwing modifier is skipped
(function () {
    'use strict';
    var red = window.__red;
    if (!red || red.hooks) {
        return;
    }
    var jsonModifiers = [];      // fn(obj, url) -> obj|undefined
    var requestModifiers = [];   // fn(url, bodyObj) -> bodyObj|undefined  (XHR/fetch /youtubei/ bodies)
    var initialHooks = {};       // name -> [fn(obj)]

    function runJson(obj, url) {
        if (!obj || typeof obj !== 'object') {
            return obj;
        }
        for (var i = 0; i < jsonModifiers.length; i++) {
            try {
                var out = jsonModifiers[i](obj, url || '');
                if (out !== undefined) {
                    obj = out;
                }
            } catch (e) {
                red.report('hooks.json', e);
            }
        }
        return obj;
    }

    function runRequest(url, body) {
        if (!body || typeof body !== 'object') {
            return body;
        }
        for (var i = 0; i < requestModifiers.length; i++) {
            try {
                var out = requestModifiers[i](url || '', body);
                if (out !== undefined) {
                    body = out;
                }
            } catch (e) {
                red.report('hooks.request', e);
            }
        }
        return body;
    }

    // --- JSON.parse ---------------------------------------------------------
    var nativeParse = JSON.parse;
    JSON.parse = function (text, reviver) {
        var result = nativeParse.call(JSON, text, reviver);
        return runJson(result, '');
    };

    // --- fetch --------------------------------------------------------------
    if (window.fetch && window.Response && Response.prototype.json) {
        var nativeFetch = window.fetch;
        window.fetch = function (input, init) {
            try {
                var url = typeof input === 'string' ? input : (input && input.url) || '';
                if (init && typeof init.body === 'string' && url.indexOf('/youtubei/') >= 0 && requestModifiers.length) {
                    var parsed = nativeParse(init.body);
                    var modified = runRequest(url, parsed);
                    init = Object.assign({}, init, { body: JSON.stringify(modified) });
                }
            } catch (e) {
                // not JSON: leave the request untouched
            }
            return nativeFetch.call(this, input, init);
        };
        var nativeJson = Response.prototype.json;
        Response.prototype.json = function () {
            var url = this.url || '';
            return nativeJson.call(this).then(function (obj) { return runJson(obj, url); });
        };
    }

    // --- XMLHttpRequest -----------------------------------------------------
    if (window.XMLHttpRequest) {
        var xhrOpen = XMLHttpRequest.prototype.open;
        var xhrSend = XMLHttpRequest.prototype.send;
        XMLHttpRequest.prototype.open = function (method, url) {
            this.__redUrl = typeof url === 'string' ? url : String(url);
            return xhrOpen.apply(this, arguments);
        };
        XMLHttpRequest.prototype.send = function (body) {
            var xhr = this;
            var url = xhr.__redUrl || '';
            if (typeof body === 'string' && url.indexOf('/youtubei/') >= 0 && requestModifiers.length) {
                try {
                    body = JSON.stringify(runRequest(url, nativeParse(body)));
                } catch (e) {
                    // not JSON
                }
            }
            if (url.indexOf('/youtubei/') >= 0 || url.indexOf('/tv_config') >= 0) {
                xhr.addEventListener('readystatechange', function () {
                    if (xhr.readyState !== 4 || xhr.__redDone) {
                        return;
                    }
                    xhr.__redDone = true;
                    if (xhr.responseType && xhr.responseType !== 'text') {
                        return;
                    }
                    var text;
                    try { text = xhr.responseText; } catch (e) { return; }
                    if (!text) {
                        return;
                    }
                    // /tv_config is newline-delimited; only its last line is JSON.
                    var prefix = '';
                    var jsonText = text;
                    if (url.indexOf('/tv_config') >= 0) {
                        var cut = text.lastIndexOf('\n');
                        prefix = text.substring(0, cut + 1);
                        jsonText = text.substring(cut + 1);
                    }
                    var obj;
                    try { obj = nativeParse(jsonText); } catch (e) { return; }
                    var out = prefix + JSON.stringify(runJson(obj, url));
                    Object.defineProperty(xhr, 'responseText', { get: function () { return out; }, configurable: true });
                    Object.defineProperty(xhr, 'response', { get: function () { return out; }, configurable: true });
                }, true); // capture: runs before the page's own listeners
            }
            return xhrSend.call(xhr, body);
        };
    }

    // --- inline globals (ytInitialPlayerResponse / ytInitialData) -----------
    function hookInitial(name) {
        if (initialHooks[name]) {
            return;
        }
        initialHooks[name] = [];
        var value = window[name];
        try {
            Object.defineProperty(window, name, {
                configurable: true,
                enumerable: true,
                get: function () { return value; },
                set: function (v) {
                    value = runJson(v, 'initial:' + name);
                    initialHooks[name].forEach(function (fn) {
                        try { fn(value); } catch (e) { red.report('hooks.initial', e); }
                    });
                }
            });
        } catch (e) {
            red.report('hooks.defineProperty', e);
        }
    }

    red.hooks = {
        onJson: function (fn) { jsonModifiers.push(fn); },
        onRequest: function (fn) { requestModifiers.push(fn); },
        onInitial: function (name, fn) { hookInitial(name); initialHooks[name].push(fn); },
        nativeParse: nativeParse,
        // Small helpers shared by the pruning scripts.
        isYouTubeApi: function (url, endpoint) {
            return typeof url === 'string' && url.indexOf('/youtubei/v1/' + endpoint) >= 0;
        },
        // Removes array elements for which predicate(el) is true, recursively,
        // down to `depth` levels. Cheap enough for InnerTube responses (~20k nodes).
        prune: function (node, predicate, depth) {
            if (depth <= 0 || !node || typeof node !== 'object') {
                return;
            }
            if (Array.isArray(node)) {
                for (var i = node.length - 1; i >= 0; i--) {
                    var el = node[i];
                    if (el && typeof el === 'object' && predicate(el)) {
                        node.splice(i, 1);
                    } else {
                        red.hooks.prune(el, predicate, depth - 1);
                    }
                }
                return;
            }
            var keys = Object.keys(node);
            for (var k = 0; k < keys.length; k++) {
                var v = node[keys[k]];
                if (v && typeof v === 'object') {
                    red.hooks.prune(v, predicate, depth - 1);
                }
            }
        },
        // Deletes every property named in `names` (an object used as a set)
        // anywhere below `node`; for renderers that arrive as a property
        // (a pop-up), not as a feed entry.
        dropKeys: function (node, names, depth) {
            if (depth <= 0 || !node || typeof node !== 'object') {
                return;
            }
            if (Array.isArray(node)) {
                for (var i = 0; i < node.length; i++) {
                    red.hooks.dropKeys(node[i], names, depth - 1);
                }
                return;
            }
            var keys = Object.keys(node);
            for (var k = 0; k < keys.length; k++) {
                if (names[keys[k]]) {
                    delete node[keys[k]];
                } else if (node[keys[k]] && typeof node[keys[k]] === 'object') {
                    red.hooks.dropKeys(node[keys[k]], names, depth - 1);
                }
            }
        }
    };
    hookInitial('ytInitialPlayerResponse');
    hookInitial('ytInitialData');
})();
