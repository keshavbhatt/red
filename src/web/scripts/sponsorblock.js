// name:     sponsorblock
// purpose:  skip / mark SponsorBlock segments for the current video (FEATURES
//           B6, T10). Segments come from C++ over the bridge (ADR-007) so the
//           page's connect-src CSP is never involved.
// depends:  __red.bridge (requestSponsorSegments / sponsorSegments);
//           yt-navigate-finish (desktop) / hashchange (TV); the <video>
//           element; .ytp-progress-bar for markers (desktop only)
// verified: 2026-09-06
// on-fail:  no-op
(function () {
    'use strict';
    var red = window.__red;
    if (!red) {
        return;
    }
    var config = red.config.sponsorBlock || {};
    red.onConfig(function (c) {
        config = c.sponsorBlock || {};
        if (!config.enabled) {
            segments = [];
            clearMarkers();
        } else if (currentId) {
            request(currentId);
        }
        if (!config.markers) {
            clearMarkers();
        } else {
            renderMarkers();
        }
    });
    var segments = [];
    var currentId = '';
    var video = null;
    var toastTimer = null;
    var lastSkipUuid = '';

    function videoIdFromLocation() {
        try {
            if (red.isTv()) {
                var route = location.hash.charAt(0) === '#' ? location.hash.substring(1) : location.hash;
                var u = new URL(route, location.href);
                return u.pathname === '/watch' ? (u.searchParams.get('v') || '') : '';
            }
            var url = new URL(location.href);
            if (url.pathname === '/watch') {
                return url.searchParams.get('v') || '';
            }
            var m = url.pathname.match(/^\/(?:shorts|live|embed)\/([A-Za-z0-9_-]{11})/);
            return m ? m[1] : '';
        } catch (e) {
            return '';
        }
    }

    function request(id) {
        red.onBridge(function (bridge) { bridge.requestSponsorSegments(id); });
    }

    function onNavigate() {
        var id = videoIdFromLocation();
        if (id === currentId) {
            attachVideo();
            return;
        }
        currentId = id;
        segments = [];
        lastSkipUuid = '';
        clearMarkers();
        attachVideo();
        if (id && config.enabled) {
            request(id);
        }
    }

    function attachVideo() {
        var v = document.querySelector('#movie_player video, .html5-video-player video, video');
        if (v === video) {
            return;
        }
        if (video) {
            video.removeEventListener('timeupdate', onTimeUpdate);
            video.removeEventListener('durationchange', renderMarkers);
        }
        video = v;
        if (video) {
            video.addEventListener('timeupdate', onTimeUpdate);
            video.addEventListener('durationchange', renderMarkers);
        }
    }

    function onTimeUpdate() {
        if (!video || video.paused || !segments.length) {
            return;
        }
        var t = video.currentTime;
        for (var i = 0; i < segments.length; i++) {
            var s = segments[i];
            // Skip only when we just entered the segment (within 1 s of its start),
            // so a manual seek into a segment is not fought.
            if (s.action === 'skip' && t >= s.start && t < s.end && t - s.start < 1.0 && s.uuid !== lastSkipUuid) {
                lastSkipUuid = s.uuid;
                video.currentTime = s.end;
                toast(s.category);
                red.onBridge(function (bridge) { bridge.sponsorSkipped(s.category); });
                return;
            }
        }
    }

    var LABELS = {
        sponsor: 'sponsor', selfpromo: 'self-promotion', interaction: 'interaction reminder',
        intro: 'intro', outro: 'outro', preview: 'preview', music_offtopic: 'non-music section', filler: 'filler'
    };
    var COLORS = {
        sponsor: '#00d400', selfpromo: '#ffff00', interaction: '#cc00ff', intro: '#00ffff',
        outro: '#0202ed', preview: '#008fd6', music_offtopic: '#ff9900', filler: '#7300ff'
    };

    function toast(category) {
        try {
            var text = 'Skipped ' + (LABELS[category] || category);
            if (red.isTv() && red.tv && red.tv.toast) {
                red.tv.toast(text, 'SponsorBlock');
                return;
            }
            var el = document.getElementById('red-sb-toast');
            if (!el) {
                el = document.createElement('div');
                el.id = 'red-sb-toast';
                el.setAttribute('style', [
                    'position:fixed', 'left:50%', 'bottom:96px', 'transform:translateX(-50%)',
                    'background:rgba(28,28,28,.92)', 'color:#f1f1f1', 'font:500 14px Roboto,Arial,sans-serif',
                    'padding:10px 16px', 'border-radius:8px', 'z-index:2147483646', 'pointer-events:none',
                    'box-shadow:0 4px 16px rgba(0,0,0,.4)', 'transition:opacity .2s', 'opacity:0'
                ].join(';'));
                document.body.appendChild(el);
            }
            el.textContent = text;
            el.style.opacity = '1';
            clearTimeout(toastTimer);
            toastTimer = setTimeout(function () { el.style.opacity = '0'; }, 2500);
        } catch (e) {
            red.report('sponsorblock.toast', e);
        }
    }

    // --- progress-bar markers (desktop) ------------------------------------------
    var markerHost = null;
    function clearMarkers() {
        if (markerHost && markerHost.parentNode) {
            markerHost.parentNode.removeChild(markerHost);
        }
        markerHost = null;
    }
    function renderMarkers() {
        if (red.isTv() || !config.markers || !segments.length || !video || !isFinite(video.duration) || video.duration <= 0) {
            return;
        }
        var bar = document.querySelector('#movie_player .ytp-progress-bar');
        if (!bar) {
            return;
        }
        clearMarkers();
        markerHost = document.createElement('div');
        markerHost.id = 'red-sb-markers';
        markerHost.setAttribute('style', 'position:absolute;left:0;top:0;width:100%;height:100%;pointer-events:none;z-index:31');
        var d = video.duration;
        for (var i = 0; i < segments.length; i++) {
            var s = segments[i];
            var m = document.createElement('div');
            var left = Math.max(0, Math.min(100, (s.start / d) * 100));
            var width = Math.max(0.2, Math.min(100 - left, ((s.end - s.start) / d) * 100));
            m.setAttribute('style', 'position:absolute;top:0;height:100%;opacity:.7;left:' + left + '%;width:' + width + '%;background:' + (COLORS[s.category] || '#fff'));
            m.title = LABELS[s.category] || s.category;
            markerHost.appendChild(m);
        }
        bar.appendChild(markerHost);
    }

    red.onBridge(function (bridge) {
        bridge.sponsorSegments.connect(function (videoId, list) {
            if (videoId !== currentId) {
                return;
            }
            segments = Array.isArray(list) ? list : [];
            red.log('sponsorblock: ' + segments.length + ' segments for ' + videoId);
            attachVideo();
            renderMarkers();
        });
    });

    window.addEventListener('yt-navigate-finish', onNavigate);
    window.addEventListener('hashchange', onNavigate);
    document.addEventListener('DOMContentLoaded', onNavigate);
    // The player element can be replaced without a navigation (e.g. mini player).
    document.addEventListener('loadedmetadata', function (e) {
        if (e.target && e.target.tagName === 'VIDEO') {
            attachVideo();
            renderMarkers();
        }
    }, true);
})();
