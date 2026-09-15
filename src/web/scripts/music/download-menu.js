// name:     music/download-menu
// purpose:  YouTube Music's menus carry a Premium-only "Download" entry; in
//           Music mode it downloads through Red instead of opening the upsell.
// depends:  ytmusic-menu-popup-renderer; the entry's own element
//           (ytmusic-menu-service-item-download-renderer, offlineVideoEndpoint)
//           or, for other shapes, icon.iconType OFFLINE_DOWNLOAD / the glyph and
//           the neighbours' queueAddEndpoint ids; the bridge's downloadRequested
// verified: 2026-09-15 against music.youtube.com
// on-fail:  no-op; YouTube Music shows its own dialog
(function () {
    'use strict';
    var red = window.__red;
    if (!red || !red.isMusic || !red.isMusic()) {
        return;
    }
    var DOWNLOAD_ITEM = 'ytmusic-menu-service-item-download-renderer';
    var ITEMS = DOWNLOAD_ITEM + ', ytmusic-menu-navigation-item-renderer, ytmusic-menu-service-item-renderer, ' +
                'ytmusic-toggle-menu-service-item-renderer';

    // Language-independent: the item's InnerTube icon type (OFFLINE_DOWNLOAD),
    // or the download glyph itself (arrow into a tray) when the type is missing.
    var GLYPH = /M17 18v1H6v-1h11z|M19 9h-4V3H9v6H5l7 7 7-7/;
    function isDownloadItem(item) {
        if (item.matches(DOWNLOAD_ITEM)) {
            return true; // the entry has its own renderer, whatever the label says
        }
        var d = item.data || {};
        var icon = (d.icon && d.icon.iconType) || '';
        if (icon) {
            return /DOWNLOAD|OFFLINE/i.test(icon);
        }
        var path = item.querySelector('yt-icon path, svg path');
        return !!(path && GLYPH.test(path.getAttribute('d') || ''));
    }
    // The Download entry names nothing; its neighbours name the song or list.
    function targetOf(popup, clicked) {
        var own = (clicked.data && clicked.data.serviceEndpoint) || {};
        var offline = own.offlineVideoEndpoint || own.offlinePlaylistEndpoint || {};
        if (offline.videoId) {
            return 'https://www.youtube.com/watch?v=' + offline.videoId;
        }
        if (offline.playlistId) {
            return 'https://www.youtube.com/playlist?list=' + offline.playlistId;
        }
        var items = popup.querySelectorAll(ITEMS);
        var playlist = '';
        for (var i = 0; i < items.length; i++) {
            var d = items[i].data || {};
            var ep = d.navigationEndpoint || d.serviceEndpoint || d.defaultServiceEndpoint || {};
            var queue = ep.queueAddEndpoint && ep.queueAddEndpoint.queueTarget;
            if (queue && queue.videoId) {
                return 'https://www.youtube.com/watch?v=' + queue.videoId;
            }
            if (ep.watchEndpoint && ep.watchEndpoint.videoId) {
                return 'https://www.youtube.com/watch?v=' + ep.watchEndpoint.videoId;
            }
            if (ep.removeFromQueueEndpoint && ep.removeFromQueueEndpoint.videoId) {
                return 'https://www.youtube.com/watch?v=' + ep.removeFromQueueEndpoint.videoId;
            }
            if (queue && queue.playlistId && !playlist) {
                playlist = queue.playlistId;
            }
        }
        return playlist ? 'https://www.youtube.com/playlist?list=' + playlist : '';
    }
    function closeMenu(popup) {
        var dropdown = popup.closest('tp-yt-iron-dropdown');
        if (dropdown && typeof dropdown.close === 'function') {
            dropdown.close();
        }
    }
    document.addEventListener('click', function (ev) {
        try {
            var item = ev.target && ev.target.closest && ev.target.closest(ITEMS);
            if (!item || !isDownloadItem(item)) {
                return;
            }
            var popup = item.closest('ytmusic-menu-popup-renderer');
            var url = popup ? targetOf(popup, item) : '';
            if (!url) {
                return;
            }
            ev.preventDefault();
            ev.stopImmediatePropagation();
            red.log('music: download menu entry -> Red: ' + url);
            closeMenu(popup);
            red.onBridge(function (bridge) { bridge.downloadRequested(url); });
        } catch (e) {
            red.report('music.download-menu', e);
        }
    }, true);
})();
