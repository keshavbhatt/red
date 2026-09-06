// name:     media-session
// purpose:  report what is playing to C++ (tray, MPRIS, screen inhibit,
//           SponsorBlock — ADR-007) and execute media commands from C++.
// depends:  the main <video> element; navigator.mediaSession.metadata (set by
//           YouTube); the public player API on #movie_player /
//           .html5-video-player (getVideoData, nextVideo, previousVideo, seekTo)
// verified: 2026-09-06
// on-fail:  no-op (no media state → tray/MPRIS show nothing)
(function () {
    'use strict';
    var red = window.__red;
    if (!red) {
        return;
    }
    var video = null;
    var lastSent = '';
    var tick = null;

    function player() {
        return document.getElementById('movie_player') || document.querySelector('.html5-video-player');
    }
    function findVideo() {
        var p = player();
        return (p && p.querySelector('video')) || document.querySelector('video.html5-main-video') || null;
    }
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
    function metadata() {
        var md = navigator.mediaSession && navigator.mediaSession.metadata;
        var out = { title: '', artist: '', artwork: '' };
        if (md) {
            out.title = md.title || '';
            out.artist = md.artist || '';
            if (md.artwork && md.artwork.length) {
                out.artwork = md.artwork[md.artwork.length - 1].src || '';
            }
        }
        var p = player();
        if ((!out.title || !out.artist) && p && typeof p.getVideoData === 'function') {
            try {
                var data = p.getVideoData();
                out.title = out.title || data.title || '';
                out.artist = out.artist || data.author || '';
                if (!out.videoId) {
                    out.videoId = data.video_id || '';
                }
            } catch (e) { /* ignore */ }
        }
        if (!out.title) {
            out.title = document.title.replace(/ - YouTube( TV)?$/, '');
        }
        return out;
    }
    function state() {
        var v = video;
        var md = metadata();
        var id = videoIdFromLocation() || md.videoId || '';
        var playing = v && !v.paused && !v.ended;
        // Off a watch page the player is parked (still holds the last video):
        // report nothing unless YouTube's own miniplayer is active.
        var parked = !id && !document.querySelector('ytd-miniplayer[active]') && !red.isTv();
        var s = {
            state: !v || parked || (!id && v.readyState === 0) ? 'none' : (playing ? 'playing' : 'paused'),
            videoId: id,
            title: md.title,
            artist: md.artist,
            artwork: md.artwork || (id ? 'https://i.ytimg.com/vi/' + id + '/mqdefault.jpg' : ''),
            duration: v && isFinite(v.duration) ? v.duration : 0,
            position: v ? v.currentTime : 0,
            live: !!(v && v.duration === Infinity),
            canGoNext: !!document.querySelector('.ytp-next-button, .ytp-next-button-modern'),
            canGoPrevious: !!document.querySelector('.ytp-prev-button')
        };
        return s;
    }
    function send(force) {
        try {
            var s = state();
            var key = JSON.stringify([s.state, s.videoId, s.title, s.artist, s.artwork, Math.round(s.duration)]);
            // Position is sent with every report; the key only decides whether
            // paused/idle reports are worth repeating.
            if (!force && key === lastSent && s.state !== 'playing') {
                return;
            }
            lastSent = key;
            red.onBridge(function (bridge) { bridge.mediaState(s); });
        } catch (e) {
            red.report('media-session.send', e);
        }
    }
    function startTick() {
        if (tick) {
            return;
        }
        tick = setInterval(function () {
            if (video && !video.paused) {
                send(false);
            }
        }, 1000);
    }
    var EVENTS = ['play', 'pause', 'ended', 'emptied', 'loadedmetadata', 'durationchange', 'seeked', 'ratechange'];
    function attach() {
        var v = findVideo();
        if (v === video) {
            return;
        }
        if (video) {
            EVENTS.forEach(function (ev) { video.removeEventListener(ev, onEvent); });
        }
        video = v;
        if (video) {
            EVENTS.forEach(function (ev) { video.addEventListener(ev, onEvent); });
            startTick();
        }
        send(true);
    }
    function onEvent() {
        // Metadata lands a moment after "play"; report twice.
        send(true);
        setTimeout(function () { send(true); }, 800);
    }

    // --- commands from C++ ------------------------------------------------------
    function command(name, arg) {
        try {
            var p = player();
            var v = video || findVideo();
            switch (name) {
            case 'play':
                if (p && p.playVideo) { p.playVideo(); } else if (v) { v.play(); }
                break;
            case 'pause':
                if (p && p.pauseVideo) { p.pauseVideo(); } else if (v) { v.pause(); }
                break;
            case 'playPause':
                if (v && v.paused) { command('play'); } else { command('pause'); }
                break;
            case 'stop':
                if (p && p.stopVideo) { p.stopVideo(); } else if (v) { v.pause(); }
                break;
            case 'next':
                if (p && p.nextVideo) { p.nextVideo(); } else {
                    var n = document.querySelector('.ytp-next-button');
                    if (n) { n.click(); }
                }
                break;
            case 'previous':
                if (p && p.previousVideo) { p.previousVideo(); } else {
                    var b = document.querySelector('.ytp-prev-button');
                    if (b) { b.click(); }
                }
                break;
            case 'seek':
                if (p && p.seekTo) { p.seekTo(arg, true); } else if (v) { v.currentTime = arg; }
                break;
            case 'seekBy':
                if (v) { v.currentTime = Math.max(0, v.currentTime + arg); }
                break;
            case 'mute':
                if (p && p.isMuted && p.mute && p.unMute) { if (p.isMuted()) { p.unMute(); } else { p.mute(); } }
                else if (v) { v.muted = !v.muted; }
                break;
            case 'volume':
                if (p && p.setVolume) { p.setVolume(Math.round(arg * 100)); } else if (v) { v.volume = arg; }
                break;
            default:
                break;
            }
            setTimeout(function () { send(true); }, 150);
        } catch (e) {
            red.report('media-session.command', e);
        }
    }
    red.onBridge(function (bridge) {
        bridge.mediaCommand.connect(command);
        send(true);
    });

    document.addEventListener('DOMContentLoaded', attach);
    window.addEventListener('yt-navigate-finish', function () { attach(); send(true); });
    window.addEventListener('hashchange', function () { attach(); send(true); });
    document.addEventListener('loadedmetadata', function (e) {
        if (e.target && e.target.tagName === 'VIDEO') {
            attach();
        }
    }, true);
    document.addEventListener('play', function (e) {
        if (e.target && e.target.tagName === 'VIDEO') {
            attach();
        }
    }, true);
})();
