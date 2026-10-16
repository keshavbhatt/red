#include "web/error_page.h"

using namespace Qt::StringLiterals;

namespace red::web {

QString errorPageHtml(bool dark, const QString& title, const QString& detail)
{
    // Red's brand tokens (DESIGN.md §1).
    const QString bg = dark ? u"#0f0f0f"_s : u"#ffffff"_s;
    const QString fg = dark ? u"#f1f1f1"_s : u"#0f0f0f"_s;
    const QString muted = dark ? u"#aaaaaa"_s : u"#606060"_s;
    // YouTube's roles (ADR-017): red for the mark, the inverted pill for the button.
    const QString brand = u"#ff0033"_s;
    const QString ring = dark ? u"rgba(255,0,51,0.16)"_s : u"rgba(255,0,51,0.12)"_s;
    const QString primary = dark ? u"#f1f1f1"_s : u"#0f0f0f"_s;
    const QString primaryHover = dark ? u"#d9d9d9"_s : u"#272727"_s;
    const QString primaryText = dark ? u"#0f0f0f"_s : u"#ffffff"_s;

    QString html = uR"HTML(<!DOCTYPE html>
<html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Red</title>
<style>
  html,body{height:100%;margin:0}
  body{background:{{BG}};color:{{FG}};
    font-family:'Segoe UI',Ubuntu,'Helvetica Neue',Helvetica,Arial,sans-serif;
    display:flex;align-items:center;justify-content:center;text-align:center;
    -webkit-user-select:none;user-select:none}
  .wrap{max-width:420px;padding:32px 24px}
  .mark{width:96px;height:96px;border-radius:24px;margin:0 auto 26px;
    background:{{BRAND}};box-shadow:0 0 0 12px {{RING}};
    display:flex;align-items:center;justify-content:center}
  .mark svg{width:52px;height:52px;fill:none;stroke:#fff;stroke-width:6;
    stroke-linecap:round;stroke-linejoin:round}
  h1{font-size:22px;font-weight:600;margin:0 0 10px}
  p{font-size:15px;line-height:1.5;color:{{MUTED}};margin:0 0 26px}
  button{appearance:none;border:0;cursor:pointer;
    background:{{PRIMARY}};color:{{PRIMARYTEXT}};font-size:15px;font-weight:600;
    padding:12px 30px;border-radius:24px;transition:background .15s ease}
  button:hover{background:{{PRIMARYHOVER}}}
  button:active{transform:translateY(1px)}
</style></head>
<body>
  <div class="wrap">
    <div class="mark">
      <svg viewBox="0 0 64 64" aria-hidden="true">
        <path d="M24 18 L48 32 L24 46 Z" fill="#fff" stroke="none"/>
        <path d="M10 10 L54 54"/>
      </svg>
    </div>
    <h1>{{TITLE}}</h1>
    <p>{{DETAIL}}</p>
    <button onclick="try{window.__red.bridge.retry()}catch(e){location.href='https://www.youtube.com/'}">Try again</button>
  </div>
</body></html>)HTML"_s;

    html.replace(u"{{BG}}"_s, bg);
    html.replace(u"{{FG}}"_s, fg);
    html.replace(u"{{MUTED}}"_s, muted);
    html.replace(u"{{BRAND}}"_s, brand);
    html.replace(u"{{PRIMARY}}"_s, primary);
    html.replace(u"{{PRIMARYHOVER}}"_s, primaryHover);
    html.replace(u"{{PRIMARYTEXT}}"_s, primaryText);
    html.replace(u"{{RING}}"_s, ring);
    html.replace(u"{{TITLE}}"_s, title.toHtmlEscaped());
    html.replace(u"{{DETAIL}}"_s, detail.toHtmlEscaped());
    return html;
}

} // namespace red::web
