// name:     hide-shorts
// purpose:  remove Shorts shelves and entries from feeds (FEATURES D9, T11),
//           matched by renderer shape, never by localised text (LESSONS V6).
// depends:  __red.hooks; reelShelfRenderer / richShelfRenderer /
//           shortsLockupViewModel / reelItemRenderer; guide entries by href
// verified: 2026-09-06
// on-fail:  no-op
(function () {
    'use strict';
    var red = window.__red;
    if (!red || !red.hooks) {
        return;
    }
    var enabled = !!red.config.hideShorts;
    red.onConfig(function (c) { enabled = !!c.hideShorts; applyCss(); });

    function shelfHasShorts(shelf) {
        var items = shelf && shelf.contents;
        if (!Array.isArray(items) && shelf && shelf.content && shelf.content.horizontalListRenderer) {
            items = shelf.content.horizontalListRenderer.items;
        }
        if (!Array.isArray(items) || !items.length) {
            return false;
        }
        var first = items[0];
        if (!first) {
            return false;
        }
        if (first.shortsLockupViewModel || first.reelItemRenderer) {
            return true;
        }
        var inner = first.richItemRenderer && first.richItemRenderer.content;
        if (inner && (inner.shortsLockupViewModel || inner.reelItemRenderer)) {
            return true;
        }
        // TV: tiles of shorts carry a reelWatchEndpoint.
        var tile = first.tileRenderer;
        if (tile && tile.onSelectCommand && tile.onSelectCommand.reelWatchEndpoint) {
            return true;
        }
        return false;
    }
    function isShorts(el) {
        if (el.reelShelfRenderer) {
            return true;
        }
        if (el.richSectionRenderer && el.richSectionRenderer.content &&
            shelfHasShorts(el.richSectionRenderer.content.richShelfRenderer)) {
            return true;
        }
        if (el.richItemRenderer && el.richItemRenderer.content &&
            (el.richItemRenderer.content.shortsLockupViewModel || el.richItemRenderer.content.reelItemRenderer)) {
            return true;
        }
        if (el.shelfRenderer && shelfHasShorts(el.shelfRenderer)) {
            return true;
        }
        if (el.itemSectionRenderer && Array.isArray(el.itemSectionRenderer.contents) &&
            el.itemSectionRenderer.contents.length === 1 && el.itemSectionRenderer.contents[0].reelShelfRenderer) {
            return true;
        }
        // Guide entries (desktop and TV) linking to /shorts.
        var guide = el.guideEntryRenderer;
        if (guide && guide.navigationEndpoint && guide.navigationEndpoint.browseEndpoint &&
            guide.navigationEndpoint.browseEndpoint.browseId === 'FEshorts') {
            return true;
        }
        if (guide && guide.icon && /SHORTS/.test(guide.icon.iconType || '')) {
            return true; // TAB_SHORTS_CAIRO (desktop), YOUTUBE_SHORTS_FILL_24 (TV)
        }
        return false;
    }
    red.hooks.onJson(function (obj) {
        if (!enabled) {
            return;
        }
        if (obj.contents || obj.onResponseReceivedActions || obj.onResponseReceivedEndpoints ||
            obj.continuationContents || obj.items) {
            red.hooks.prune(obj, isShorts, 24);
        }
    });

    var CSS = [
        'ytd-rich-shelf-renderer[is-shorts], ytd-reel-shelf-renderer, ytd-rich-section-renderer:has(ytd-rich-shelf-renderer[is-shorts]),',
        'ytd-rich-item-renderer:has(> #content > ytm-shorts-lockup-view-model-v2), ytd-rich-item-renderer:has(ytm-shorts-lockup-view-model),',
        'ytd-guide-entry-renderer:has(> a[href^="/shorts"]), ytd-mini-guide-entry-renderer:has(> a[href^="/shorts"]),',
        'ytd-guide-entry-renderer:has(yt-icon[icon$="SHORTS"]), ytd-mini-guide-entry-renderer:has(yt-icon[icon$="SHORTS"]),',
        'ytd-video-renderer:has(a[href^="/shorts/"]), ytd-grid-video-renderer:has(a[href^="/shorts/"])',
        '{ display: none !important; }'
    ].join('\n');
    function applyCss() {
        try {
            if (red.isTv()) {
                return;
            }
            if (enabled) {
                red.style('red-hide-shorts', CSS);
            } else {
                red.unstyle('red-hide-shorts');
            }
        } catch (e) {
            red.report('hide-shorts.css', e);
        }
    }
    applyCss();
})();
