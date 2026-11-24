#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include <windows.h>
#include <windowsx.h>
#include <objbase.h>
#include <shellapi.h>
#include <string>
#include <vector>
#include <memory>
#include <algorithm>
#include <cwctype>
#include <cstdio>
#include <cstddef>
#include <fstream>
#include <filesystem>

#include "WebView2.h"

namespace {

bool IsSystemDarkMode() {
    DWORD value = 0;
    DWORD size = sizeof(value);
    HKEY hKey;
    if (RegOpenKeyExA(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        RegQueryValueExA(hKey, "AppsUseLightTheme", nullptr, nullptr, reinterpret_cast<LPBYTE>(&value), &size);
        RegCloseKey(hKey);
    }
    return value == 0;
}

COLORREF GetToolbarColor() {
    COLORREF toolbarColor = GetSysColor(COLOR_BTNFACE);
    
    HKEY hKey;
    DWORD value = 0;
    DWORD size = sizeof(value);
    if (RegOpenKeyExA(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        if (RegQueryValueExA(hKey, "SystemUsesLightTheme", nullptr, nullptr, reinterpret_cast<LPBYTE>(&value), &size) == ERROR_SUCCESS) {
            if (value == 0) {
                toolbarColor = RGB(30, 30, 30);
            }
        }
        RegCloseKey(hKey);
    }
    
    return toolbarColor;
}

COLORREF CalculateBackgroundColor(COLORREF toolbarColor, bool darkMode) {
    if (darkMode) {
        BYTE r = GetRValue(toolbarColor);
        BYTE g = GetGValue(toolbarColor);
        BYTE b = GetBValue(toolbarColor);
        r = static_cast<BYTE>(r * 0.8);
        g = static_cast<BYTE>(g * 0.8);
        b = static_cast<BYTE>(b * 0.8);
        return RGB(r, g, b);
    } else {
        BYTE r = GetRValue(toolbarColor);
        BYTE g = GetGValue(toolbarColor);
        BYTE b = GetBValue(toolbarColor);
        r = static_cast<BYTE>(std::min(255, static_cast<int>(r) + 20));
        g = static_cast<BYTE>(std::min(255, static_cast<int>(g) + 20));
        b = static_cast<BYTE>(std::min(255, static_cast<int>(b) + 20));
        return RGB(r, g, b);
    }
}

HICON LoadCustomIcon() {
    static HICON icon = LoadIconW(nullptr, IDI_APPLICATION);
    return icon;
}

constexpr wchar_t kWindowClass[] = L"CustomBrowserWindow";
constexpr wchar_t kWindowTitle[] = L"Custom Browser";
constexpr int kTabHeight = 34;
constexpr int kNavTop = kTabHeight;
constexpr int kNavHeight = 42;
constexpr int kContentTop = kNavTop + kNavHeight;

constexpr int IDC_BACK = 1001;
constexpr int IDC_FORWARD = 1002;
constexpr int IDC_RELOAD = 1003;
constexpr int IDC_HOME = 1004;
constexpr int IDC_NEW_TAB = 1005;
constexpr int IDC_ADDRESS = 1006;
constexpr int IDC_GO = 1007;
constexpr int IDC_SEARCH_ENGINE = 1008;
constexpr int IDC_SETTINGS = 1009;
constexpr int IDC_TIME = 1010;
constexpr int kSearchEngineMenuBase = 2000;
constexpr int kSettingsMenuBase = 3000;
constexpr int ID_SETTINGS_CLEAR_DATA = 3000;
constexpr int ID_SETTINGS_CLEAR_HISTORY = 3001;
constexpr int ID_SETTINGS_THEME_LIGHT = 3002;
constexpr int ID_SETTINGS_THEME_DARK = 3003;
constexpr int ID_SETTINGS_TIMEZONE = 3004;
constexpr int kTimezoneMenuBase = 4000;

enum class SearchEngine {
    Google = 0,
    Bing = 1,
    DuckDuckGo = 2,
    Yahoo = 3,
    Yandex = 4,
    Ecosia = 5,
    AOL = 6,
    InternetArchive = 7,
    Searxng = 8,
    InternetExplorer = 9,
    Excite = 10,
    Infoseek = 11,
    WebCrawler = 12,
    Lycos = 13,
    Dogpile = 14,
    AltaVista = 15,
    Mojeek = 16,
    StartPage = 17,
    WolframAlpha = 18,
    MetaGer = 19,
    Swisscows = 20,
    Qwant = 21,
    Seznam = 22,
};

struct SearchEngineDef {
    const wchar_t* displayName;
    const char* settingName;
    const wchar_t* searchUrlPrefix;
    const wchar_t* homeUrl;
};

constexpr SearchEngineDef kSearchEngineDefs[] = {
    {L"Google", "google", L"https://www.google.com/search?q=", L"https://www.google.com"},
    {L"Bing", "bing", L"https://www.bing.com/search?q=", L"https://www.bing.com"},
    {L"DuckDuckGo", "duckduckgo", L"https://duckduckgo.com/?q=", L"https://duckduckgo.com"},
    {L"Yahoo", "yahoo", L"https://search.yahoo.com/search?p=", L"https://search.yahoo.com"},
    {L"Yandex", "yandex", L"https://yandex.com/search/?text=", L"https://yandex.com"},
    {L"Ecosia", "ecosia", L"https://www.ecosia.org/search?q=", L"https://www.ecosia.org"},
    {L"AOL", "aol", L"https://search.aol.com/aol/search?q=", L"https://search.aol.com"},
    {L"Internet Archive", "internetarchive", L"https://archive.org/search?query=", L"https://archive.org"},
    {L"SearXNG", "searxng", nullptr, nullptr},
    {L"Internet Explorer", "internetexplorer", nullptr, nullptr},
    {L"Excite (1995)", "excite", nullptr, L""},
    {L"Infoseek (1994)", "infoseek", nullptr, L""},
    {L"Web Crawler (1994)", "webcrawler", L"https://www.webcrawler.com/?q=", L"https://www.webcrawler.com"},
    {L"Lycos (1994)", "lycos", nullptr, L""},
    {L"Dogpile", "dogpile", L"https://www.dogpile.com/serp?q=", L"https://www.dogpile.com"},
    {L"AltaVista (1995)", "altavista", nullptr, L""},
    {L"Mojeek", "mojeek", L"https://www.mojeek.com/search?q=", L"https://www.mojeek.com"},
    {L"StartPage", "startpage", L"https://www.startpage.com/sp/search?query=", L"https://www.startpage.com"},
    {L"WolframAlpha", "wolframalpha", L"https://www.wolframalpha.com/input?i=", L"https://www.wolframalpha.com"},
    {L"MetaGer", "metager", L"https://metager.org/meta/meta.ger3?eingabe=", L"https://metager.org"},
    {L"Swisscows", "swisscows", L"https://swisscows.com/web?query=", L"https://swisscows.com"},
    {L"Qwant", "qwant", L"https://www.qwant.com/?q=", L"https://www.qwant.com"},
    {L"Seznam", "seznam", L"https://search.seznam.cz/?q=", L"https://search.seznam.cz"},
};
constexpr int kSearchEngineCount = static_cast<int>(sizeof(kSearchEngineDefs) / sizeof(kSearchEngineDefs[0]));

std::string NarrowToUtf8(const std::wstring& value) {
    int length = WideCharToMultiByte(CP_UTF8, 0, value.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (length <= 1) {
        return {};
    }
    std::string out(static_cast<size_t>(length), '\0');
    WideCharToMultiByte(CP_UTF8, 0, value.c_str(), -1, out.data(), length, nullptr, nullptr);
    out.pop_back();
    return out;
}

std::string HtmlEscape(const std::string& value) {
    std::string out;
    out.reserve(value.size());
    for (char c : value) {
        switch (c) {
        case '&': out += "&amp;"; break;
        case '<': out += "&lt;"; break;
        case '>': out += "&gt;"; break;
        case '"': out += "&quot;"; break;
        case '\'': out += "&#39;"; break;
        default: out.push_back(c); break;
        }
    }
    return out;
}

std::string BuildIeStartPageHtml() {
    return std::string(R"HTML(<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<title>Internet Explorer</title>
<style>
  html, body { margin: 0; padding: 0; height: 100%; font-family: Tahoma, "Segoe UI", sans-serif; background: #fff; }
  #commandbar { background: linear-gradient(#fdfdfd, #ececec); border-bottom: 1px solid #d4d4d4; padding: 4px 12px; font-size: 12px; color: #333; }
  #commandbar span { margin-right: 22px; cursor: default; }
  #commandbar span:hover { text-decoration: underline; }
  #toplinks { background: #f5f5f5; border-bottom: 1px solid #e2e2e2; padding: 5px 14px; font-size: 12px; }
  #toplinks a { color: #444; text-decoration: none; margin-right: 18px; }
  #toplinks a:hover { text-decoration: underline; }
  #main { display: flex; flex-direction: column; align-items: center; padding-top: 50px; }
  #logo { margin-bottom: 14px; }
  #iedition { font-size: 26px; color: #1a1a1a; margin-bottom: 20px; }
  #searchbar { margin-bottom: 20px; }
  #searchbar form { display: flex; gap: 8px; }
  #searchbar input { width: 300px; padding: 4px 8px; font-size: 13px; border: 1px solid #999; font-family: Tahoma, sans-serif; }
  #searchbar button { padding: 4px 16px; font-size: 12px; background: linear-gradient(#fbfbfb, #e0e0e0); border: 1px solid #999; cursor: pointer; font-family: Tahoma, sans-serif; }
  #searchbar button:hover { background: linear-gradient(#ffffff, #f0f0f0); }
  #addresshint { font-size: 12px; color: #666; }
  #footer { position: absolute; bottom: 16px; width: 100%; text-align: center; font-size: 11px; color: #999; }
</style>
</head>
<body>
<div id="commandbar"><span>Favorites</span><span>Tools</span><span>Safety</span></div>
<div id="toplinks">
  <a href="https://www.msn.com">MSN</a>
  <a href="https://www.bing.com/news">News</a>
  <a href="https://www.bing.com/sports">Sports</a>
  <a href="https://www.bing.com/weather">Weather</a>
  <a href="https://outlook.live.com">Mail</a>
  <a href="https://www.office.com">Office</a>
</div>
<div id="main">
  <div id="logo">
    <svg width="88" height="88" viewBox="0 0 32 32">
      <ellipse cx="16" cy="16" rx="14.5" ry="5.6" fill="none" stroke="#f7b500" stroke-width="2.2" transform="rotate(-16 16 16)"/>
      <circle cx="16" cy="16" r="9.6" fill="#0072c6"/>
      <text x="16" y="20.4" font-family="Segoe UI, Tahoma, sans-serif" font-size="12.5" font-weight="bold" fill="#ffffff" text-anchor="middle">e</text>
    </svg>
  </div>
  <div id="iedition">Internet Explorer</div>
  <div id="searchbar">
    <form action="https://www.bing.com/search" method="get">
      <input type="text" name="q" placeholder="Search the web" autofocus autocomplete="off">
      <input type="hidden" name="ensearch" value="1">
      <button type="submit">Search</button>
    </form>
  </div>
  <div id="addresshint">Or type a web address in the address bar above</div>
</div>
<div id="footer">Recreated Internet Explorer 9 start page</div>
</body>
</html>
)HTML");
}

std::string BuildIeResultsPageHtml(const std::wstring& query) {
    std::string q = HtmlEscape(NarrowToUtf8(query));
    std::string bingUrl = "https://www.bing.com/search?q=" + q + "&ensearch=1";

    return std::string(R"HTML(<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<title>Bing - Internet Explorer</title>
<style>
  html, body { margin: 0; padding: 0; height: 100%; font-family: Tahoma, "Segoe UI", sans-serif; background: #fff; }
  #header { background: linear-gradient(#fdfdfd, #ececec); border-bottom: 1px solid #d4d4d4; padding: 8px 16px; display: flex; align-items: center; gap: 20px; }
  #binglogo { font-size: 24px; font-weight: bold; color: #008373; }
  #searchbar { flex: 1; display: flex; gap: 8px; }
  #searchbar input { flex: 1; padding: 4px 8px; font-size: 13px; border: 1px solid #999; font-family: Tahoma, sans-serif; }
  #searchbar button { padding: 4px 16px; font-size: 12px; background: linear-gradient(#fbfbfb, #e0e0e0); border: 1px solid #999; cursor: pointer; font-family: Tahoma, sans-serif; }
  #searchbar button:hover { background: linear-gradient(#ffffff, #f0f0f0); }
  #main { padding: 20px 30px; }
  #resultsinfo { font-size: 12px; color: #666; margin-bottom: 15px; }
  .result { margin-bottom: 25px; padding-bottom: 15px; border-bottom: 1px solid #eee; }
  .result h3 { margin: 0 0 5px 0; font-size: 16px; }
  .result h3 a { color: #1a0dab; text-decoration: none; }
  .result h3 a:hover { text-decoration: underline; }
  .result .url { font-size: 12px; color: #006621; margin-bottom: 5px; }
  .result .snippet { font-size: 13px; color: #333; line-height: 1.4; }
  .result .cached { font-size: 11px; color: #006621; margin-top: 5px; }
  #sidebar { float: right; width: 250px; margin-left: 30px; }
  .sidebar-box { background: #f9f9f9; border: 1px solid #ddd; padding: 12px; margin-bottom: 15px; border-radius: 4px; }
  .sidebar-box h4 { margin: 0 0 10px 0; font-size: 13px; color: #333; }
  .sidebar-box a { display: block; color: #0066cc; text-decoration: none; font-size: 12px; margin: 4px 0; }
  .sidebar-box a:hover { text-decoration: underline; }
  #footer { clear: both; text-align: center; padding: 20px; font-size: 11px; color: #999; }
  #loading { text-align: center; padding: 40px; color: #666; }
</style>
</head>
<body>
<div id="header">
  <div id="binglogo">Bing</div>
  <div id="searchbar">
    <form action="https://www.bing.com/search" method="get">
      <input type="text" name="q" value=")HTML" + q + R"HTML(" placeholder="Search">
      <input type="hidden" name="ensearch" value="1">
      <button type="submit">Search</button>
    </form>
  </div>
</div>
<div id="main">
  <div id="resultsinfo">About )HTML" + q + R"HTML( results</div>
  <div id="loading">Loading results...</div>
  <div id="results"></div>
</div>
<div id="sidebar">
  <div class="sidebar-box">
    <h4>Related Searches</h4>
    <a href="https://www.bing.com/search?q=)HTML" + q + R"HTML(+review&ensearch=1">)HTML" + q + R"HTML( review</a>
    <a href="https://www.bing.com/search?q=)HTML" + q + R"HTML(+news&ensearch=1">)HTML" + q + R"HTML( news</a>
    <a href="https://www.bing.com/search?q=)HTML" + q + R"HTML(+images&ensearch=1">)HTML" + q + R"HTML( images</a>
  </div>
  <div class="sidebar-box">
    <h4>Quick Links</h4>
    <a href=")HTML" + bingUrl + R"HTML(">View on Bing</a>
    <a href="https://www.google.com/search?q=)HTML" + q + R"HTML(">Search on Google</a>
  </div>
</div>
<div id="footer">Internet Explorer - Bing Search Results</div>
<script>
fetch(')HTML" + bingUrl + R"HTML(')
  .then(r => r.text())
  .then(html => {
    const parser = new DOMParser();
    const doc = parser.parseFromString(html, 'text/html');
    const results = doc.querySelectorAll('li.b_algo');
    const container = document.getElementById('results');
    const loading = document.getElementById('loading');
    loading.style.display = 'none';
    
    if (results.length === 0) {
      container.innerHTML = '<div class="result"><h3><a href=")HTML" + bingUrl + R"HTML(">No results found - click to view on Bing</a></h3></div>';
      return;
    }
    
    results.forEach((r, i) => {
      if (i >= 10) return;
      const h2 = r.querySelector('h2');
      const a = h2 ? h2.querySelector('a') : null;
      const cite = r.querySelector('.b_attribution cite, .b_caption cite');
      const snippet = r.querySelector('.b_caption p, p');
      
      if (a) {
        const div = document.createElement('div');
        div.className = 'result';
        div.innerHTML = `
          <h3><a href="${a.href}" target="_blank">${a.textContent}</a></h3>
          <div class="url">${cite ? cite.textContent : a.href}</div>
          <div class="snippet">${snippet ? snippet.textContent : ''}</div>
        `;
        container.appendChild(div);
      }
    });
  })
  .catch(e => {
    document.getElementById('loading').innerHTML = '<div class="result"><h3><a href=")HTML" + bingUrl + R"HTML(">Click here to view results on Bing</a></h3><div class="url">www.bing.com</div><div class="snippet">Could not load results automatically.</div></div>';
  });
</script>
</body>
</html>
)HTML");
}

std::string BuildExciteStartPageHtml() {
    return std::string(R"HTML(<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<title>Excite</title>
<style>
  body { margin: 0; padding: 0; background: #FFFFFF; font-family: Arial, Helvetica, sans-serif; }
  .header { background: #FFCC00; padding: 10px; }
  .header table { width: 100%; }
  .logo { text-align: center; font-size: 24px; font-weight: bold; color: #000066; }
  .nav { text-align: right; font-size: 12px; padding: 5px; }
  .nav a { color: #000066; text-decoration: none; margin: 0 10px; }
  .content { padding: 20px; }
  .search-box { text-align: center; margin: 30px 0; }
  .search-box input { width: 300px; padding: 5px; font-size: 14px; border: 1px solid #000; }
  .search-box button { padding: 5px 15px; font-size: 14px; background: #FFCC00; border: 1px solid #000; cursor: pointer; }
  .links { display: flex; justify-content: space-around; margin: 30px 0; flex-wrap: wrap; }
  .link-group { text-align: center; margin: 10px; }
  .link-group h3 { color: #000066; font-size: 14px; margin-bottom: 10px; }
  .link-group a { display: block; color: #000066; text-decoration: none; font-size: 12px; margin: 5px 0; }
  .news { background: #F0F0F0; padding: 15px; margin: 20px 0; border: 1px solid #CCC; }
  .news h3 { color: #000066; font-size: 14px; margin-bottom: 10px; }
  .news-item { font-size: 12px; margin: 5px 0; }
  .footer { text-align: center; padding: 20px; font-size: 11px; color: #666; }
</style>
</head>
<body>
<div class="header">
  <table>
    <tr>
      <td class="logo">Excite</td>
      <td class="nav">
        <a href="#">Login</a> |
        <a href="#">Account Info</a> |
        <a href="#">Member Directory</a> |
        <a href="#">Help</a>
      </td>
    </tr>
  </table>
</div>
<div class="content">
  <div class="search-box">
    <form action="https://www.google.com/search" method="get">
      <input type="text" name="q" placeholder="Search Excite" autofocus>
      <button type="submit">Search</button>
    </form>
  </div>
  <div class="links">
    <div class="link-group">
      <h3>Personalize Your Page!</h3>
      <a href="#">FREE Excite Mail Account</a>
      <a href="#">Make Excite My Start Page</a>
    </div>
    <div class="link-group">
      <h3>Services</h3>
      <a href="#">Airline Tickets</a>
      <a href="#">Auctions</a>
      <a href="#">Chat</a>
      <a href="#">Custom News</a>
      <a href="#">Email Lookup</a>
      <a href="#">Excite Cam</a>
      <a href="#">Job Finder</a>
      <a href="#">Maps & Directions</a>
      <a href="#">People Finder</a>
      <a href="#">Personals</a>
      <a href="#">Start a Biz</a>
      <a href="#">Share Photos</a>
      <a href="#">Stock Quotes</a>
      <a href="#">Weather</a>
      <a href="#">Yellow Pages</a>
      <a href="#">More...</a>
    </div>
    <div class="link-group">
      <h3>Categories</h3>
      <a href="#">Autos</a>
      <a href="#">Careers</a>
      <a href="#">City Guides</a>
      <a href="#">Classifieds & Auctions</a>
      <a href="#">Computers & Internet</a>
      <a href="#">Education</a>
      <a href="#">Entertainment</a>
      <a href="#">Health</a>
      <a href="#">Horoscopes</a>
      <a href="#">Home & Real Estate</a>
      <a href="#">Lifestyle</a>
      <a href="#">Money & Investing</a>
      <a href="#">News</a>
      <a href="#">People & Chat</a>
      <a href="#">Shopping</a>
      <a href="#">Sports</a>
      <a href="#">Travel</a>
    </div>
  </div>
  <div class="news">
    <h3>Today on Excite</h3>
    <div class="news-item">• Win $5,000 Cash!</div>
    <div class="news-item">• Top Restaurant Reviews</div>
    <div class="news-item">• Excite Yellow Pages</div>
    <div class="news-item">• Financial Fitness Quiz</div>
  </div>
</div>
<div class="footer">Recreated Excite 1999 interface - Search powered by Google</div>
</body>
</html>
)HTML");
}

std::string BuildInfoseekStartPageHtml() {
    return std::string(R"HTML(<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<title>Infoseek</title>
<style>
  body { margin: 0; padding: 0; background: #FFFFFF; font-family: Arial, Helvetica, sans-serif; }
  .header { background: #FFFF00; padding: 10px; border-bottom: 2px solid #000; }
  .header table { width: 100%; }
  .logo { text-align: center; font-size: 28px; font-weight: bold; color: #000; }
  .nav { text-align: right; font-size: 12px; padding: 5px; }
  .nav a { color: #000; text-decoration: none; margin: 0 10px; }
  .content { padding: 20px; }
  .search-box { text-align: center; margin: 30px 0; }
  .search-box input { width: 300px; padding: 5px; font-size: 14px; border: 2px solid #000; }
  .search-box button { padding: 5px 15px; font-size: 14px; background: #FFFF00; border: 2px solid #000; cursor: pointer; }
  .links { display: flex; justify-content: space-around; margin: 30px 0; flex-wrap: wrap; }
  .link-group { text-align: center; margin: 10px; }
  .link-group h3 { color: #000; font-size: 14px; margin-bottom: 10px; font-weight: bold; }
  .link-group a { display: block; color: #000; text-decoration: none; font-size: 12px; margin: 5px 0; }
  .news { background: #FFFFCC; padding: 15px; margin: 20px 0; border: 1px solid #000; }
  .news h3 { color: #000; font-size: 14px; margin-bottom: 10px; font-weight: bold; }
  .news-item { font-size: 12px; margin: 5px 0; }
  .footer { text-align: center; padding: 20px; font-size: 11px; color: #666; }
</style>
</head>
<body>
<div class="header">
  <table>
    <tr>
      <td class="logo">Infoseek</td>
      <td class="nav">
        <a href="#">People Finder</a> |
        <a href="#">Shareware</a> |
        <a href="#">Company Capsules</a> |
        <a href="#">More...</a>
      </td>
    </tr>
  </table>
</div>
<div class="content">
  <div class="search-box">
    <form action="https://www.google.com/search" method="get">
      <input type="text" name="q" placeholder="Search Infoseek" autofocus>
      <button type="submit">Search</button>
    </form>
  </div>
  <div class="links">
    <div class="link-group">
      <h3>Infoseek Today</h3>
      <a href="#">House hunting?</a>
      <a href="#">Catch a flick</a>
      <a href="#">Weekend sports scores</a>
      <a href="#">Quit your stupid job</a>
    </div>
    <div class="link-group">
      <h3>Categories</h3>
      <a href="#">Automotive</a>
      <a href="#">Business</a>
      <a href="#">Careers</a>
      <a href="#">Communications</a>
      <a href="#">Computer</a>
      <a href="#">Education</a>
      <a href="#">Entertainment</a>
      <a href="#">The Good Life</a>
      <a href="#">Health</a>
      <a href="#">Internet</a>
      <a href="#">Kids & Family</a>
      <a href="#">News</a>
      <a href="#">Personal Finance</a>
      <a href="#">Real Estate</a>
      <a href="#">Shopping</a>
      <a href="#">Sports</a>
      <a href="#">Travel</a>
      <a href="#">Women's</a>
    </div>
    <div class="link-group">
      <h3>Infoseek Tools</h3>
      <a href="#">Extra Search Precision</a>
      <a href="#">Web Sites</a>
      <a href="#">News</a>
      <a href="#">Companies</a>
      <a href="#">Newsgroups</a>
      <a href="#">Search Tips</a>
      <a href="#">Advanced Search</a>
    </div>
  </div>
  <div class="news">
    <h3>News Headlines</h3>
    <div class="news-item">• House Panel Approves Second Impeachment Article</div>
    <div class="news-item">• U.S. House Panel Passes Three Impeachment Articles</div>
    <div class="news-item">• Doctor: Chances Slim For Baseball Legend DiMaggio</div>
    <div class="news-item">• Extra! Judiciary Committee approves second impeachment article</div>
  </div>
</div>
<div class="footer">Recreated Infoseek 1998 interface - Search powered by Google</div>
</body>
</html>
)HTML");
}

std::string BuildLycosStartPageHtml() {
    return std::string(R"HTML(<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<title>Lycos</title>
<style>
  body { margin: 0; padding: 0; background: #FFFFFF; font-family: Arial, Helvetica, sans-serif; }
  .header { background: #0066CC; padding: 15px; text-align: center; }
  .logo { font-size: 32px; font-weight: bold; color: #FFFFFF; margin-bottom: 10px; }
  .tagline { font-size: 14px; color: #FFFFFF; margin-bottom: 20px; }
  .content { padding: 30px; text-align: center; }
  .search-box { margin: 30px 0; }
  .search-box input { width: 400px; padding: 8px; font-size: 16px; border: 2px solid #0066CC; }
  .search-box button { padding: 8px 20px; font-size: 16px; background: #0066CC; color: #FFFFFF; border: none; cursor: pointer; }
  .stats { background: #F0F8FF; padding: 20px; margin: 30px auto; max-width: 600px; border: 1px solid #0066CC; }
  .stats h3 { color: #0066CC; font-size: 16px; margin-bottom: 10px; }
  .stats p { font-size: 14px; color: #333; margin: 5px 0; }
  .links { display: flex; justify-content: center; margin: 40px 0; flex-wrap: wrap; gap: 30px; }
  .link-group { text-align: center; }
  .link-group h3 { color: #0066CC; font-size: 16px; margin-bottom: 15px; }
  .link-group a { display: block; color: #333; text-decoration: none; font-size: 14px; margin: 8px 0; }
  .footer { text-align: center; padding: 20px; font-size: 12px; color: #666; }
</style>
</head>
<body>
<div class="header">
  <div class="logo">Lycos</div>
  <div class="tagline">The Wolf Spider - Internet Search Engine</div>
</div>
<div class="content">
  <div class="search-box">
    <form action="https://www.google.com/search" method="get">
      <input type="text" name="q" placeholder="Search the Web with Lycos" autofocus>
      <button type="submit">Search</button>
    </form>
  </div>
  <div class="stats">
    <h3>Lycos Search Statistics</h3>
    <p>• Over 54,000 documents indexed</p>
    <p>• Probabilistic retrieval system</p>
    <p>• Relevancy-ranked results</p>
    <p>• Updated weekly by web crawler</p>
  </div>
  <div class="links">
    <div class="link-group">
      <h3>Search</h3>
      <a href="#">Web Search</a>
      <a href="#">Image Search</a>
      <a href="#">News Search</a>
      <a href="#">Advanced Search</a>
    </div>
    <div class="link-group">
      <h3>Categories</h3>
      <a href="#">Arts & Humanities</a>
      <a href="#">Business & Finance</a>
      <a href="#">Computers & Internet</a>
      <a href="#">Education</a>
      <a href="#">Entertainment</a>
      <a href="#">Health</a>
      <a href="#">News & Media</a>
      <a href="#">Science</a>
      <a href="#">Sports</a>
      <a href="#">Travel</a>
    </div>
    <div class="link-group">
      <h3>Lycos Services</h3>
      <a href="#">Lycos Mail</a>
      <a href="#">Lycos Chat</a>
      <a href="#">Lycos Games</a>
      <a href="#">Lycos Weather</a>
      <a href="#">Lycos Maps</a>
      <a href="#">Lycos Yellow Pages</a>
    </div>
  </div>
</div>
<div class="footer">Recreated Lycos 1994 interface - Search powered by Google</div>
</body>
</html>
)HTML");
}

std::string BuildAltaVistaStartPageHtml() {
    return std::string(R"HTML(<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<title>AltaVista</title>
<style>
  body { margin: 0; padding: 0; background: #FFFFFF; font-family: Arial, Helvetica, sans-serif; }
  .header { background: #000080; padding: 15px; text-align: center; }
  .logo { font-size: 36px; font-weight: bold; color: #FFFFFF; margin-bottom: 5px; }
  .tagline { font-size: 14px; color: #FFFFFF; margin-bottom: 15px; }
  .nav { text-align: center; font-size: 12px; color: #FFFFFF; }
  .nav a { color: #FFFFFF; text-decoration: none; margin: 0 15px; }
  .content { padding: 30px; text-align: center; }
  .search-box { margin: 30px 0; }
  .search-box input { width: 400px; padding: 8px; font-size: 16px; border: 2px solid #000080; }
  .search-box button { padding: 8px 20px; font-size: 16px; background: #000080; color: #FFFFFF; border: none; cursor: pointer; }
  .stats { background: #F0F8FF; padding: 20px; margin: 30px auto; max-width: 600px; border: 1px solid #000080; }
  .stats h3 { color: #000080; margin: 0 0 10px 0; }
  .stats p { font-size: 14px; color: #333; margin: 5px 0; }
  .link-group { text-align: center; margin: 20px; }
  .link-group h3 { color: #000080; font-size: 14px; margin-bottom: 10px; }
  .link-group a { display: block; color: #000080; text-decoration: none; font-size: 12px; margin: 5px 0; }
  .footer { text-align: center; padding: 20px; font-size: 11px; color: #666; }
</style>
</head>
<body>
<div class="header">
  <div class="logo">AltaVista</div>
  <div class="tagline">The Most Powerful and Useful Guide to the Net</div>
  <div class="nav">
    <a href="#">Add URL</a> |
    <a href="#">Services</a> |
    <a href="#">Help</a> |
    <a href="#">About AltaVista</a>
  </div>
</div>
<div class="content">
  <div class="search-box">
    <form action="https://www.google.com/search" method="get">
      <input type="text" name="q" placeholder="Search the Web with AltaVista" autofocus autocomplete="off">
      <button type="submit">Search</button>
    </form>
  </div>
  <div class="stats">
    <h3>AltaVista Statistics</h3>
    <p>30 million Web pages indexed</p>
    <p>Over 14,000 news groups</p>
    <p>Fast search response time</p>
  </div>
  <div class="link-group">
    <h3>Popular Categories</h3>
    <a href="#">Business</a>
    <a href="#">Computers</a>
    <a href="#">Entertainment</a>
    <a href="#">Health</a>
    <a href="#">News</a>
    <a href="#">Sports</a>
    <a href="#">Travel</a>
  </div>
  <div class="link-group">
    <h3>AltaVista Services</h3>
    <a href="#">AltaVista Shopping</a>
    <a href="#">AltaVista Images</a>
    <a href="#">AltaVista Audio</a>
    <a href="#">AltaVista Video</a>
    <a href="#">AltaVista News</a>
  </div>
</div>
<div class="footer">Recreated AltaVista 1995 interface - Search powered by Google</div>
</body>
</html>
)HTML");
}

std::string BuildExciteResultsPageHtml(const std::wstring& query) {
    std::string q = HtmlEscape(NarrowToUtf8(query));
    std::string googleUrl = "https://www.google.com/search?q=" + q;

    return std::string(R"HTML(<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<title>Excite Search Results</title>
<style>
  body { margin: 0; padding: 0; background: #FFFFFF; font-family: Arial, Helvetica, sans-serif; }
  .header { background: #FFCC00; padding: 10px; }
  .header table { width: 100%; }
  .logo { text-align: center; font-size: 24px; font-weight: bold; color: #000066; }
  .search-bar { text-align: center; padding: 15px; background: #FFF8DC; }
  .search-bar input { width: 300px; padding: 5px; font-size: 14px; border: 1px solid #000; }
  .search-bar button { padding: 5px 15px; font-size: 14px; background: #FFCC00; border: 1px solid #000; cursor: pointer; }
  .content { padding: 20px; }
  .results-info { font-size: 12px; color: #666; margin-bottom: 15px; }
  .result { margin-bottom: 20px; padding-bottom: 15px; border-bottom: 1px solid #DDD; }
  .result h3 { margin: 0 0 5px 0; font-size: 16px; }
  .result h3 a { color: #000066; text-decoration: none; }
  .result h3 a:hover { text-decoration: underline; }
  .result .url { font-size: 12px; color: #008000; margin-bottom: 5px; }
  .result .snippet { font-size: 13px; color: #333; line-height: 1.4; }
  .sidebar { float: right; width: 200px; margin-left: 20px; }
  .sidebar-box { background: #F5F5F5; border: 1px solid #CCC; padding: 10px; margin-bottom: 15px; }
  .sidebar-box h4 { margin: 0 0 8px 0; font-size: 13px; color: #000066; }
  .sidebar-box a { display: block; color: #000066; text-decoration: none; font-size: 12px; margin: 3px 0; }
  .footer { clear: both; text-align: center; padding: 20px; font-size: 11px; color: #666; }
</style>
</head>
<body>
<div class="header">
  <table><tr><td class="logo">Excite</td></tr></table>
</div>
<div class="search-bar">
  <form action="https://www.google.com/search" method="get">
    <input type="text" name="q" value=")HTML" + q + R"HTML(" placeholder="Search Excite">
    <button type="submit">Search</button>
  </form>
</div>
<div class="content">
  <div class="results-info">Search results for: )HTML" + q + R"HTML(</div>
  <div class="sidebar">
    <div class="sidebar-box">
      <h4>Refine Search</h4>
      <a href="https://www.google.com/search?q=)HTML" + q + R"HTML(+images">Images</a>
      <a href="https://www.google.com/search?q=)HTML" + q + R"HTML(+news">News</a>
      <a href="https://www.google.com/search?q=)HTML" + q + R"HTML(+video">Video</a>
    </div>
    <div class="sidebar-box">
      <h4>Search Tips</h4>
      <p style="font-size: 11px; margin: 0;">Use + to require terms, - to exclude.</p>
    </div>
  </div>
  <div id="results"><div style="text-align:center;padding:20px;color:#666;">Loading results...</div></div>
</div>
<div class="footer">Excite Search - Powered by Google</div>
<script>
fetch(')HTML" + googleUrl + R"HTML(')
  .then(r => r.text())
  .then(html => {
    const parser = new DOMParser();
    const doc = parser.parseFromString(html, 'text/html');
    const results = doc.querySelectorAll('div.g, div[data-sokoban-container]');
    const container = document.getElementById('results');
    
    if (results.length === 0) {
      container.innerHTML = '<div class="result"><h3><a href=")HTML" + googleUrl + R"HTML(">View results on Google</a></h3><div class="url">www.google.com</div><div class="snippet">Click here to see full results.</div></div>';
      return;
    }
    
    let count = 0;
    results.forEach(r => {
      if (count >= 10) return;
      const a = r.querySelector('a');
      const h3 = r.querySelector('h3');
      const cite = r.querySelector('cite');
      const snippet = r.querySelector('.VwiC3b, .IsZvec');
      
      if (a && h3) {
        const div = document.createElement('div');
        div.className = 'result';
        div.innerHTML = `
          <h3><a href="${a.href}" target="_blank">${h3.textContent}</a></h3>
          <div class="url">${cite ? cite.textContent : a.href}</div>
          <div class="snippet">${snippet ? snippet.textContent : ''}</div>
        `;
        container.appendChild(div);
        count++;
      }
    });
    
    if (count === 0) {
      container.innerHTML = '<div class="result"><h3><a href=")HTML" + googleUrl + R"HTML(">View results on Google</a></h3><div class="url">www.google.com</div><div class="snippet">Click here to see full results.</div></div>';
    }
  })
  .catch(e => {
    document.getElementById('results').innerHTML = '<div class="result"><h3><a href=")HTML" + googleUrl + R"HTML(">Click here to view results on Google</a></h3><div class="url">www.google.com</div><div class="snippet">Could not load results automatically.</div></div>';
  });
</script>
</body>
</html>
)HTML");
}

std::string BuildInfoseekResultsPageHtml(const std::wstring& query) {
    std::string q = HtmlEscape(NarrowToUtf8(query));
    std::string googleUrl = "https://www.google.com/search?q=" + q;

    return std::string(R"HTML(<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<title>Infoseek Search Results</title>
<style>
  body { margin: 0; padding: 0; background: #FFFFFF; font-family: Arial, Helvetica, sans-serif; }
  .header { background: #FFFF00; padding: 10px; border-bottom: 2px solid #000; }
  .header table { width: 100%; }
  .logo { text-align: center; font-size: 28px; font-weight: bold; color: #000; }
  .search-bar { text-align: center; padding: 15px; background: #FFFFCC; }
  .search-bar input { width: 300px; padding: 5px; font-size: 14px; border: 2px solid #000; }
  .search-bar button { padding: 5px 15px; font-size: 14px; background: #FFFF00; border: 2px solid #000; cursor: pointer; }
  .content { padding: 20px; }
  .results-info { font-size: 12px; color: #666; margin-bottom: 15px; font-weight: bold; }
  .result { margin-bottom: 20px; padding-bottom: 15px; border-bottom: 1px solid #000; }
  .result h3 { margin: 0 0 5px 0; font-size: 16px; font-weight: bold; }
  .result h3 a { color: #000; text-decoration: none; }
  .result h3 a:hover { text-decoration: underline; }
  .result .url { font-size: 12px; color: #008000; margin-bottom: 5px; }
  .result .snippet { font-size: 13px; color: #333; line-height: 1.4; }
  .sidebar { float: right; width: 200px; margin-left: 20px; }
  .sidebar-box { background: #FFFFCC; border: 1px solid #000; padding: 10px; margin-bottom: 15px; }
  .sidebar-box h4 { margin: 0 0 8px 0; font-size: 13px; color: #000; font-weight: bold; }
  .sidebar-box a { display: block; color: #000; text-decoration: none; font-size: 12px; margin: 3px 0; }
  .footer { clear: both; text-align: center; padding: 20px; font-size: 11px; color: #666; }
</style>
</head>
<body>
<div class="header">
  <table><tr><td class="logo">Infoseek</td></tr></table>
</div>
<div class="search-bar">
  <form action="https://www.google.com/search" method="get">
    <input type="text" name="q" value=")HTML" + q + R"HTML(" placeholder="Search Infoseek">
    <button type="submit">Search</button>
  </form>
</div>
<div class="content">
  <div class="results-info">Search results for: )HTML" + q + R"HTML(</div>
  <div class="sidebar">
    <div class="sidebar-box">
      <h4>Refine Search</h4>
      <a href="https://www.google.com/search?q=)HTML" + q + R"HTML(+images">Images</a>
      <a href="https://www.google.com/search?q=)HTML" + q + R"HTML(+news">News</a>
      <a href="https://www.google.com/search?q=)HTML" + q + R"HTML(+video">Video</a>
    </div>
  </div>
  <div id="results"><div style="text-align:center;padding:20px;color:#666;">Loading results...</div></div>
</div>
<div class="footer">Infoseek Search - Powered by Google</div>
<script>
fetch(')HTML" + googleUrl + R"HTML(')
  .then(r => r.text())
  .then(html => {
    const parser = new DOMParser();
    const doc = parser.parseFromString(html, 'text/html');
    const results = doc.querySelectorAll('div.g, div[data-sokoban-container]');
    const container = document.getElementById('results');
    
    if (results.length === 0) {
      container.innerHTML = '<div class="result"><h3><a href=")HTML" + googleUrl + R"HTML(">View results on Google</a></h3><div class="url">www.google.com</div><div class="snippet">Click here to see full results.</div></div>';
      return;
    }
    
    let count = 0;
    results.forEach(r => {
      if (count >= 10) return;
      const a = r.querySelector('a');
      const h3 = r.querySelector('h3');
      const cite = r.querySelector('cite');
      const snippet = r.querySelector('.VwiC3b, .IsZvec');
      
      if (a && h3) {
        const div = document.createElement('div');
        div.className = 'result';
        div.innerHTML = `
          <h3><a href="${a.href}" target="_blank">${h3.textContent}</a></h3>
          <div class="url">${cite ? cite.textContent : a.href}</div>
          <div class="snippet">${snippet ? snippet.textContent : ''}</div>
        `;
        container.appendChild(div);
        count++;
      }
    });
    
    if (count === 0) {
      container.innerHTML = '<div class="result"><h3><a href=")HTML" + googleUrl + R"HTML(">View results on Google</a></h3><div class="url">www.google.com</div><div class="snippet">Click here to see full results.</div></div>';
    }
  })
  .catch(e => {
    document.getElementById('results').innerHTML = '<div class="result"><h3><a href=")HTML" + googleUrl + R"HTML(">Click here to view results on Google</a></h3><div class="url">www.google.com</div><div class="snippet">Could not load results automatically.</div></div>';
  });
</script>
</body>
</html>
)HTML");
}

std::string BuildLycosResultsPageHtml(const std::wstring& query) {
    std::string q = HtmlEscape(NarrowToUtf8(query));
    std::string googleUrl = "https://www.google.com/search?q=" + q;

    return std::string(R"HTML(<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<title>Lycos Search Results</title>
<style>
  body { margin: 0; padding: 0; background: #FFFFFF; font-family: Arial, Helvetica, sans-serif; }
  .header { background: #0066CC; padding: 15px; text-align: center; }
  .logo { font-size: 32px; font-weight: bold; color: #FFFFFF; margin-bottom: 10px; }
  .tagline { font-size: 14px; color: #FFFFFF; margin-bottom: 20px; }
  .search-bar { text-align: center; padding: 20px; background: #F0F8FF; }
  .search-bar input { width: 400px; padding: 8px; font-size: 16px; border: 2px solid #0066CC; }
  .search-bar button { padding: 8px 20px; font-size: 16px; background: #0066CC; color: #FFFFFF; border: none; cursor: pointer; }
  .content { padding: 30px; }
  .results-info { font-size: 14px; color: #333; margin-bottom: 20px; text-align: center; }
  .result { margin-bottom: 25px; padding-bottom: 20px; border-bottom: 1px solid #0066CC; }
  .result h3 { margin: 0 0 8px 0; font-size: 18px; }
  .result h3 a { color: #0066CC; text-decoration: none; }
  .result h3 a:hover { text-decoration: underline; }
  .result .url { font-size: 13px; color: #008000; margin-bottom: 8px; }
  .result .snippet { font-size: 14px; color: #333; line-height: 1.5; }
  .sidebar { float: right; width: 250px; margin-left: 30px; }
  .sidebar-box { background: #F0F8FF; border: 1px solid #0066CC; padding: 15px; margin-bottom: 20px; }
  .sidebar-box h4 { margin: 0 0 10px 0; font-size: 14px; color: #0066CC; }
  .sidebar-box a { display: block; color: #0066CC; text-decoration: none; font-size: 13px; margin: 5px 0; }
  .footer { clear: both; text-align: center; padding: 20px; font-size: 12px; color: #666; }
</style>
</head>
<body>
<div class="header">
  <div class="logo">Lycos</div>
  <div class="tagline">The Wolf Spider - Internet Search Engine</div>
</div>
<div class="search-bar">
  <form action="https://www.google.com/search" method="get">
    <input type="text" name="q" value=")HTML" + q + R"HTML(" placeholder="Search the Web with Lycos">
    <button type="submit">Search</button>
  </form>
</div>
<div class="content">
  <div class="results-info">Search results for: )HTML" + q + R"HTML(</div>
  <div class="sidebar">
    <div class="sidebar-box">
      <h4>Refine Search</h4>
      <a href="https://www.google.com/search?q=)HTML" + q + R"HTML(+images">Images</a>
      <a href="https://www.google.com/search?q=)HTML" + q + R"HTML(+news">News</a>
      <a href="https://www.google.com/search?q=)HTML" + q + R"HTML(+video">Video</a>
    </div>
  </div>
  <div id="results"><div style="text-align:center;padding:20px;color:#666;">Loading results...</div></div>
</div>
<div class="footer">Lycos Search - Powered by Google</div>
<script>
fetch(')HTML" + googleUrl + R"HTML(')
  .then(r => r.text())
  .then(html => {
    const parser = new DOMParser();
    const doc = parser.parseFromString(html, 'text/html');
    const results = doc.querySelectorAll('div.g, div[data-sokoban-container]');
    const container = document.getElementById('results');
    
    if (results.length === 0) {
      container.innerHTML = '<div class="result"><h3><a href=")HTML" + googleUrl + R"HTML(">View results on Google</a></h3><div class="url">www.google.com</div><div class="snippet">Click here to see full results.</div></div>';
      return;
    }
    
    let count = 0;
    results.forEach(r => {
      if (count >= 10) return;
      const a = r.querySelector('a');
      const h3 = r.querySelector('h3');
      const cite = r.querySelector('cite');
      const snippet = r.querySelector('.VwiC3b, .IsZvec');
      
      if (a && h3) {
        const div = document.createElement('div');
        div.className = 'result';
        div.innerHTML = `
          <h3><a href="${a.href}" target="_blank">${h3.textContent}</a></h3>
          <div class="url">${cite ? cite.textContent : a.href}</div>
          <div class="snippet">${snippet ? snippet.textContent : ''}</div>
        `;
        container.appendChild(div);
        count++;
      }
    });
    
    if (count === 0) {
      container.innerHTML = '<div class="result"><h3><a href=")HTML" + googleUrl + R"HTML(">View results on Google</a></h3><div class="url">www.google.com</div><div class="snippet">Click here to see full results.</div></div>';
    }
  })
  .catch(e => {
    document.getElementById('results').innerHTML = '<div class="result"><h3><a href=")HTML" + googleUrl + R"HTML(">Click here to view results on Google</a></h3><div class="url">www.google.com</div><div class="snippet">Could not load results automatically.</div></div>';
  });
</script>
</body>
</html>
)HTML");
}

HICON IeIcon() {
    static HICON icon = [] {
        constexpr int size = 32;
        BITMAPINFO bmi{};
        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth = size;
        bmi.bmiHeader.biHeight = -size;
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 32;
        bmi.bmiHeader.biCompression = BI_RGB;

        HDC screen = GetDC(nullptr);
        HDC mem = CreateCompatibleDC(screen);
        void* bits = nullptr;
        HBITMAP color = CreateDIBSection(mem, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
        if (bits) {
            std::fill_n(static_cast<BYTE*>(bits), size * size * 4, static_cast<BYTE>(0));
        }
        HGDIOBJ oldBmp = SelectObject(mem, color);
        SetBkMode(mem, TRANSPARENT);

        HPEN orbitPen = CreatePen(PS_SOLID, 3, RGB(255, 150, 0));
        SelectObject(mem, orbitPen);
        SelectObject(mem, GetStockObject(NULL_BRUSH));
        Ellipse(mem, 2, 8, 30, 26);

        HBRUSH sphere = CreateSolidBrush(RGB(10, 100, 200));
        SelectObject(mem, sphere);
        Ellipse(mem, 8, 5, 24, 21);

        HFONT font = CreateFontW(-18, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                                 OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                 DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
        HGDIOBJ oldFont = SelectObject(mem, font);
        SetTextColor(mem, RGB(255, 255, 255));
        RECT textRect{8, 3, 24, 21};
        DrawTextW(mem, L"e", -1, &textRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        SelectObject(mem, oldFont);
        SelectObject(mem, oldBmp);
        DeleteObject(font);
        DeleteObject(sphere);
        DeleteObject(orbitPen);

        std::vector<BYTE> maskData(static_cast<size_t>(size) * 4, static_cast<BYTE>(0xFF));
        if (bits) {
            const BYTE* px = static_cast<const BYTE*>(bits);
            for (int y = 0; y < size; ++y) {
                for (int x = 0; x < size; ++x) {
                    if (px[(static_cast<size_t>(y) * size + x) * 4 + 3] > 0) {
                        maskData[static_cast<size_t>(y) * 4 + x / 8] &=
                            static_cast<BYTE>(~(0x80 >> (x % 8)));
                    }
                }
            }
        }
        HBITMAP mask = CreateBitmap(size, size, 1, 1, maskData.data());

        ICONINFO info{};
        info.fIcon = TRUE;
        info.hbmMask = mask;
        info.hbmColor = color;
        HICON result = CreateIconIndirect(&info);

        if (mask) DeleteObject(mask);
        if (color) DeleteObject(color);
        DeleteDC(mem);
        ReleaseDC(nullptr, screen);
        return result ? result : LoadIconW(nullptr, IDI_APPLICATION);
    }();
    return icon;
}

const SearchEngineDef& SearchEngineDefFor(SearchEngine engine) {
    int index = static_cast<int>(engine);
    if (index < 0 || index >= kSearchEngineCount) {
        index = 1;
    }
    return kSearchEngineDefs[index];
}

const wchar_t* SearchEngineDisplayName(SearchEngine engine) {
    return SearchEngineDefFor(engine).displayName;
}

const char* SearchEngineSettingName(SearchEngine engine) {
    return SearchEngineDefFor(engine).settingName;
}

SearchEngine SearchEngineFromIndex(int index) {
    if (index < 0 || index >= kSearchEngineCount) {
        return SearchEngine::Bing;
    }
    return static_cast<SearchEngine>(index);
}

bool TryParseSearchEngineName(const std::string& name, SearchEngine& engine) {
    for (int i = 0; i < kSearchEngineCount; ++i) {
        if (name == kSearchEngineDefs[i].settingName) {
            engine = static_cast<SearchEngine>(i);
            return true;
        }
    }
    return false;
}

std::string NarrowAscii(const std::wstring& value) {
    std::string out;
    out.reserve(value.size());
    for (wchar_t c : value) {
        out.push_back(c >= 0 && c < 128 ? static_cast<char>(c) : '?');
    }
    return out;
}

std::string TrimAscii(const std::string& value) {
    const size_t first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return {};
    }
    const size_t last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

struct Tab {
    int id = 0;
    ICoreWebView2Controller* controller = nullptr;
    ICoreWebView2* webView = nullptr;
    bool controllerRequested = false;
    bool ready = false;
    std::wstring title = L"New Tab";
    std::wstring url;
    std::wstring pendingUrl;

    ~Tab() {
        Shutdown();
    }

    void Shutdown() {
        if (controller) {
            controller->Close();
        }
        if (webView) {
            webView->Release();
            webView = nullptr;
        }
        if (controller) {
            controller->Release();
            controller = nullptr;
        }
        ready = false;
    }
};

class BrowserWindow {
public:
    explicit BrowserWindow(HINSTANCE instance) : instance_(instance) {}

    bool Create(int showCommand) {
        bool darkMode = IsSystemDarkMode();
        COLORREF toolbarColor = GetToolbarColor();
        COLORREF bgColor = CalculateBackgroundColor(toolbarColor, darkMode);
        
        HBRUSH bgBrush = CreateSolidBrush(bgColor);
        
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(wc);
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = &BrowserWindow::WndProc;
        wc.hInstance = instance_;
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.hbrBackground = bgBrush;
        wc.lpszClassName = kWindowClass;
        wc.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
        wc.hIconSm = wc.hIcon;

        if (!RegisterClassExW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
            return false;
        }

        hwnd_ = CreateWindowExW(
            0,
            kWindowClass,
            kWindowTitle,
            WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            1100,
            720,
            nullptr,
            nullptr,
            instance_,
            this);



        if (!hwnd_) {
            return false;
        }

        CreateControls();
        LoadSettings();
        InitWebView2();
        ShowWindow(hwnd_, showCommand);
        UpdateWindow(hwnd_);
        return true;
    }

    void OnEnvironmentCreated(HRESULT errorCode, ICoreWebView2Environment* environment) {
        if (shuttingDown_) {
            return;
        }

        if (FAILED(errorCode) || !environment) {
            MessageBoxW(
                hwnd_,
                L"The Microsoft Edge WebView2 Runtime is required to display web pages.\n\nInstall it from Microsoft and try again.",
                kWindowTitle,
                MB_ICONERROR | MB_OK);
            return;
        }

        environment->AddRef();
        environment_ = environment;
        environmentReady_ = true;

        for (auto& tab : tabs_) {
            EnsureController(*tab);
        }
        if (tabs_.empty()) {
            AddTab(homeUrl_);
        }
        UpdateUI();
    }

    void OnControllerCreated(int tabId, HRESULT errorCode, ICoreWebView2Controller* controller) {
        if (shuttingDown_) {
            if (controller) {
                controller->Close();
            }
            return;
        }

        Tab* tab = FindTab(tabId);
        if (FAILED(errorCode) || !controller || !tab) {
            if (tab) {
                tab->controllerRequested = false;
            }
            if (controller && !tab) {
                controller->Close();
            }
            return;
        }

        controller->AddRef();
        tab->controller = controller;

        ICoreWebView2* webView = nullptr;
        if (FAILED(controller->get_CoreWebView2(&webView)) || !webView) {
            tab->controller->Close();
            tab->controller->Release();
            tab->controller = nullptr;
            tab->controllerRequested = false;
            return;
        }
        tab->webView = webView;

        ICoreWebView2Settings* settings = nullptr;
        if (SUCCEEDED(webView->get_Settings(&settings)) && settings) {
            settings->put_AreDevToolsEnabled(TRUE);
            settings->put_AreDefaultContextMenusEnabled(TRUE);
            settings->put_AreDefaultScriptDialogsEnabled(TRUE);
            settings->put_IsStatusBarEnabled(FALSE);
            settings->Release();
        }

        RegisterWebViewEvents(*tab);
        tab->ready = true;
        ApplyTabVisibility();
        UpdateUI();

        if (!tab->pendingUrl.empty()) {
            tab->url = tab->pendingUrl;
            webView->Navigate(tab->pendingUrl.c_str());
            tab->pendingUrl.clear();
        }
    }

    void OnNavigationStarting(int tabId, ICoreWebView2* sender, ICoreWebView2NavigationStartingEventArgs* args) {
        if (!args) {
            return;
        }
        LPWSTR uri = nullptr;
        if (SUCCEEDED(args->get_Uri(&uri)) && uri) {
            std::wstring url = uri;
            CoTaskMemFree(uri);




        }
    }

    void OnNavigationCompleted(int tabId, ICoreWebView2* sender, ICoreWebView2NavigationCompletedEventArgs* args) {
        (void)sender;
        (void)args;
        OnTabStateChanged(tabId);
    }

    void OnSourceChanged(int tabId, ICoreWebView2* sender, ICoreWebView2SourceChangedEventArgs* args) {
        (void)args;
        Tab* tab = FindTab(tabId);
        if (tab && sender) {
            LPWSTR uri = nullptr;
            if (SUCCEEDED(sender->get_Source(&uri)) && uri) {
                tab->url = uri;
                CoTaskMemFree(uri);
            }
        }
        OnTabStateChanged(tabId);
    }

    void OnDocumentTitleChanged(int tabId, ICoreWebView2* sender, IUnknown* args) {
        (void)args;
        Tab* tab = FindTab(tabId);
        if (tab && sender) {
            LPWSTR title = nullptr;
            if (SUCCEEDED(sender->get_DocumentTitle(&title)) && title) {
                tab->title = title[0] ? title : L"New Tab";
                CoTaskMemFree(title);
            }
        }
        OnTabStateChanged(tabId);
    }

    void OnHistoryChanged(int tabId, ICoreWebView2* sender, IUnknown* args) {
        (void)sender;
        (void)args;
        OnTabStateChanged(tabId);
    }

    void OnNewWindowRequested(int tabId, ICoreWebView2NewWindowRequestedEventArgs* args) {
        (void)tabId;
        if (!args) {
            return;
        }

        LPWSTR uri = nullptr;
        std::wstring url;
        if (SUCCEEDED(args->get_Uri(&uri)) && uri) {
            url = uri;
            CoTaskMemFree(uri);
        }
        args->put_Handled(TRUE);
        if (url.empty()) {
            url = L"about:blank";
        }
        AddTab(url);
    }

    void NavigateFromAddressBar() {
        NavigateActive(NormalizeUrl(GetEditText(hwndAddress_)));
    }

    void GoBack() {
        Tab* tab = ActiveTab();
        if (tab && tab->webView) {
            tab->webView->GoBack();
        }
    }

    void GoForward() {
        Tab* tab = ActiveTab();
        if (tab && tab->webView) {
            tab->webView->GoForward();
        }
    }

    void Reload() {
        Tab* tab = ActiveTab();
        if (tab && tab->webView) {
            tab->webView->Reload();
        }
    }

    void GoHome() {
        NavigateActive(homeUrl_);
    }

    void OpenNewTab() {
        AddTab(homeUrl_);
    }

    void ClearUserData() {
        wchar_t localAppData[MAX_PATH]{};
        if (!GetEnvironmentVariableW(L"LOCALAPPDATA", localAppData, MAX_PATH)) {
            return;
        }
        std::wstring root = std::wstring(localAppData) + L"\\CustomBrowser";
        std::wstring settings = root + L"\\settings.txt";
        std::wstring webView2 = root + L"\\WebView2";
        std::wstring ieStart = root + L"\\ie_start.html";
        std::wstring ieResults = root + L"\\ie_results.html";

        DeleteFileW(settings.c_str());
        DeleteFileW(ieStart.c_str());
        DeleteFileW(ieResults.c_str());

        SHFILEOPSTRUCTW fileOp{};
        fileOp.wFunc = FO_DELETE;
        fileOp.pFrom = webView2.c_str();
        fileOp.fFlags = FOF_NOCONFIRMATION | FOF_NOERRORUI | FOF_SILENT;
        SHFileOperationW(&fileOp);

        searchEngine_ = SearchEngine::Bing;
        searxngUrl_ = L"https://searx.be";
        ApplySearchEngine();
        MessageBoxW(hwnd_, L"User data has been cleared.", kWindowTitle, MB_ICONINFORMATION | MB_OK);
    }

    void ClearSearchHistory() {
        for (auto& tab : tabs_) {
            if (tab->webView) {
                tab->webView->ExecuteScript(L"localStorage.clear();", nullptr);
                tab->webView->ExecuteScript(L"sessionStorage.clear();", nullptr);
            }
        }
        MessageBoxW(hwnd_, L"Search history has been cleared.", kWindowTitle, MB_ICONINFORMATION | MB_OK);
    }

private:
    HINSTANCE instance_ = nullptr;
    HWND hwnd_ = nullptr;
    HWND hwndBack_ = nullptr;
    HWND hwndForward_ = nullptr;
    HWND hwndReload_ = nullptr;
    HWND hwndHome_ = nullptr;
    HWND hwndNewTab_ = nullptr;
    HWND hwndSearchEngine_ = nullptr;
    HWND hwndAddress_ = nullptr;
    HWND hwndGo_ = nullptr;
    HWND hwndSettings_ = nullptr;
    HWND hwndTime_ = nullptr;
    WNDPROC addressProc_ = nullptr;

    ICoreWebView2Environment* environment_ = nullptr;
    bool environmentReady_ = false;
    bool shuttingDown_ = false;
    SearchEngine searchEngine_ = SearchEngine::Bing;
    std::wstring searxngUrl_ = L"https://searx.be";
    std::wstring homeUrl_ = L"https://www.bing.com";
    bool darkMode_ = false;
    std::wstring timezone_ = L"UTC";

    std::vector<std::unique_ptr<Tab>> tabs_;
    size_t activeTab_ = 0;
    int nextTabId_ = 1;

    static LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
        BrowserWindow* self = reinterpret_cast<BrowserWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));

        if (message == WM_NCCREATE) {
            auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(create->lpCreateParams));
            return DefWindowProcW(hwnd, message, wParam, lParam);
        }

        if (!self) {
            return DefWindowProcW(hwnd, message, wParam, lParam);
        }

        switch (message) {
        case WM_CREATE:
            SetTimer(hwnd, 1, 1000, nullptr);
            return 0;

        case WM_TIMER:
            if (wParam == 1) {
                self->UpdateTimeDisplay();
            }
            return 0;

        case WM_SIZE:
            self->LayoutControls();
            self->ApplyTabVisibility();
            return 0;

        case WM_GETMINMAXINFO: {
            auto* info = reinterpret_cast<MINMAXINFO*>(lParam);
            info->ptMinTrackSize.x = 640;
            info->ptMinTrackSize.y = 480;
            return 0;
        }

        case WM_ERASEBKGND:
            return 1;

        case WM_PAINT:
            self->Paint();
            return 0;

        case WM_LBUTTONDOWN:
        case WM_MBUTTONDOWN: {
            int x = GET_X_LPARAM(lParam);
            int y = GET_Y_LPARAM(lParam);
            bool close = false;
            int index = self->HitTestTab(x, y, close);
            if (index >= 0) {
                if (close || message == WM_MBUTTONDOWN) {
                    self->CloseTab(static_cast<size_t>(index));
                } else {
                    self->SelectTab(static_cast<size_t>(index));
                }
                return 0;
            }
            break;
        }

        case WM_COMMAND: {
            int id = LOWORD(wParam);
            int notification = HIWORD(wParam);
            if (notification == BN_CLICKED || (id == IDC_ADDRESS && notification == EN_SETFOCUS)) {
                switch (id) {
                case IDC_BACK: self->GoBack(); break;
                case IDC_FORWARD: self->GoForward(); break;
                case IDC_RELOAD: self->Reload(); break;
                case IDC_HOME: self->GoHome(); break;
                case IDC_NEW_TAB: self->OpenNewTab(); break;
                case IDC_SEARCH_ENGINE: self->ShowSearchEngineMenu(); break;
                case IDC_GO: self->NavigateFromAddressBar(); break;
                case IDC_SETTINGS: self->ShowSettingsMenu(); break;
                default: break;
                }
                if (id != IDC_ADDRESS) {
                    return 0;
                }
            }
            break;
        }

        case WM_DESTROY:
            self->shuttingDown_ = true;
            self->tabs_.clear();
            if (self->environment_) {
                self->environment_->Release();
                self->environment_ = nullptr;
            }
            self->environmentReady_ = false;
            PostQuitMessage(0);
            return 0;

        default:
            break;
        }

        return DefWindowProcW(hwnd, message, wParam, lParam);
    }

    static LRESULT CALLBACK AddressEditProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
        BrowserWindow* self = reinterpret_cast<BrowserWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        if (message == WM_KEYDOWN && wParam == VK_RETURN) {
            if (self) {
                self->NavigateFromAddressBar();
            }
            return 0;
        }
        WNDPROC oldProc = self ? self->addressProc_ : nullptr;
        return CallWindowProcW(oldProc, hwnd, message, wParam, lParam);
    }

    void CreateControls() {
        hwndBack_ = CreateWindowW(L"BUTTON", L"<", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            0, 0, 0, 0, hwnd_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_BACK)), instance_, nullptr);
        hwndForward_ = CreateWindowW(L"BUTTON", L">", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            0, 0, 0, 0, hwnd_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_FORWARD)), instance_, nullptr);
        hwndReload_ = CreateWindowW(L"BUTTON", L"R", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            0, 0, 0, 0, hwnd_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_RELOAD)), instance_, nullptr);
        hwndHome_ = CreateWindowW(L"BUTTON", L"H", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            0, 0, 0, 0, hwnd_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_HOME)), instance_, nullptr);
        hwndNewTab_ = CreateWindowW(L"BUTTON", L"+", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            0, 0, 0, 0, hwnd_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_NEW_TAB)), instance_, nullptr);
        hwndSearchEngine_ = CreateWindowW(L"BUTTON", L"Search: Bing", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            0, 0, 0, 0, hwnd_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_SEARCH_ENGINE)), instance_, nullptr);
        hwndAddress_ = CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL | ES_LEFT,
            0, 0, 0, 0, hwnd_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_ADDRESS)), instance_, nullptr);
        hwndGo_ = CreateWindowW(L"BUTTON", L"Go", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            0, 0, 0, 0, hwnd_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_GO)), instance_, nullptr);
        hwndSettings_ = CreateWindowW(L"BUTTON", L"Settings", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            0, 0, 0, 0, hwnd_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_SETTINGS)), instance_, nullptr);
        hwndTime_ = CreateWindowW(L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_CENTER,
            0, 0, 0, 0, hwnd_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_TIME)), instance_, nullptr);

        HFONT hFont = CreateFontW(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
        if (hFont) {
            SendMessageW(hwndTime_, WM_SETFONT, reinterpret_cast<WPARAM>(hFont), TRUE);
        }

        SetWindowLongPtrW(hwndAddress_, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
        addressProc_ = reinterpret_cast<WNDPROC>(SetWindowLongPtrW(hwndAddress_, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&BrowserWindow::AddressEditProc)));

        LayoutControls();
    }

    void InitWebView2();

    void EnsureController(Tab& tab);
    void RegisterWebViewEvents(Tab& tab);

    void AddTab(const std::wstring& url, bool activate = true) {
        auto tab = std::make_unique<Tab>();
        tab->id = nextTabId_++;
        tab->url = url.empty() ? homeUrl_ : url;
        tab->pendingUrl = tab->url;
        tabs_.push_back(std::move(tab));

        if (activate) {
            activeTab_ = tabs_.size() - 1;
        }

        EnsureController(*tabs_.back());
        ApplyTabVisibility();
        UpdateUI();
    }

    void CloseTab(size_t index) {
        if (index >= tabs_.size()) {
            return;
        }
        if (tabs_.size() == 1) {
            PostMessageW(hwnd_, WM_CLOSE, 0, 0);
            return;
        }

        bool wasActive = index == activeTab_;
        size_t fallback = activeTab_;
        if (wasActive) {
            fallback = index == 0 ? 0 : index - 1;
        } else if (index < activeTab_) {
            fallback = activeTab_ - 1;
        }

        tabs_.erase(tabs_.begin() + static_cast<std::ptrdiff_t>(index));
        activeTab_ = std::min(fallback, tabs_.size() - 1);
        ApplyTabVisibility();
        UpdateUI();
    }

    void SelectTab(size_t index) {
        if (index >= tabs_.size() || index == activeTab_) {
            return;
        }
        activeTab_ = index;
        ApplyTabVisibility();
        UpdateUI();
    }

    Tab* FindTab(int id) {
        for (auto& tab : tabs_) {
            if (tab->id == id) {
                return tab.get();
            }
        }
        return nullptr;
    }

    Tab* ActiveTab() {
        if (activeTab_ >= tabs_.size()) {
            return nullptr;
        }
        return tabs_[activeTab_].get();
    }

    bool IsActiveTab(const Tab* tab) const {
        return tab && activeTab_ < tabs_.size() && tabs_[activeTab_].get() == tab;
    }

    void OnTabStateChanged(int tabId) {
        Tab* tab = FindTab(tabId);
        if (!tab) {
            return;
        }
        UpdateNavButtons();
        if (IsActiveTab(tab)) {
            UpdateAddressBar();
        }
        RefreshTabs();
    }

    void NavigateActive(const std::wstring& url) {
        if (tabs_.empty()) {
            AddTab(url);
            return;
        }

        Tab* tab = ActiveTab();
        if (!tab) {
            return;
        }

        std::wstring target = url;
        if (IsIeMode() && url.find(L"bing.com/search") != std::wstring::npos) {
            size_t qpos = url.find(L"q=");
            if (qpos != std::wstring::npos) {
                qpos += 2;
                size_t end = url.find(L"&", qpos);
                std::wstring query = url.substr(qpos, end == std::wstring::npos ? std::wstring::npos : end - qpos);
                for (size_t i = 0; i < query.size(); ++i) {
                    if (query[i] == L'+') query[i] = L' ';
                    else if (query[i] == L'%') {
                        if (i + 2 < query.size()) {
                            int hi = query[i+1], lo = query[i+2];
                            int val = (hi <= '9' ? hi - '0' : hi <= 'F' ? hi - 'A' + 10 : hi - 'a' + 10) * 16 +
                                      (lo <= '9' ? lo - '0' : lo <= 'F' ? lo - 'A' + 10 : lo - 'a' + 10);
                            query[i] = static_cast<wchar_t>(val);
                            query.erase(i+1, 2);
                        }
                    }
                }
                EnsureIeResultsPage(query);
                target = IeResultsPageUrl();
            }
        }

        tab->url = target;
        if (tab->webView) {
            tab->webView->Navigate(target.c_str());
        } else {
            tab->pendingUrl = target;
        }
        UpdateUI();
    }

    void UpdateUI() {
        UpdateWindowTitle();
        UpdateAddressBar();
        UpdateNavButtons();
        RefreshTabs();
    }

    void UpdateAddressBar() {
        if (!hwndAddress_) {
            return;
        }
        Tab* tab = ActiveTab();
        std::wstring text;
        if (tab) {
            if (!tab->url.empty()) {
                text = tab->url;
            } else if (!tab->pendingUrl.empty()) {
                text = tab->pendingUrl;
            }
        }
        if (text.empty()) {
            text = homeUrl_;
        }
        SetWindowTextW(hwndAddress_, text.c_str());
    }

    void UpdateNavButtons() {
        Tab* tab = ActiveTab();
        BOOL canGoBack = FALSE;
        BOOL canGoForward = FALSE;
        if (tab && tab->webView) {
            tab->webView->get_CanGoBack(&canGoBack);
            tab->webView->get_CanGoForward(&canGoForward);
        }
        EnableWindow(hwndBack_, canGoBack ? TRUE : FALSE);
        EnableWindow(hwndForward_, canGoForward ? TRUE : FALSE);
    }

    void RefreshTabs() {
        if (!hwnd_) {
            return;
        }
        RECT rc{};
        GetClientRect(hwnd_, &rc);
        RECT strip{0, 0, rc.right, kTabHeight};
        InvalidateRect(hwnd_, &strip, FALSE);
    }

    RECT ContentRect() const {
        RECT rc{};
        if (hwnd_) {
            GetClientRect(hwnd_, &rc);
        }
        RECT content{0, kContentTop, rc.right, std::max(kContentTop, static_cast<int>(rc.bottom))};
        return content;
    }

    void LayoutControls() {
        RECT rc{};
        GetClientRect(hwnd_, &rc);
        const int width = rc.right;
        const int y = kNavTop + 5;
        const int h = 32;

        int addressX = 380;
        if (IsIeMode()) {
            MoveWindow(hwndBack_, 8, y, 62, h, TRUE);
            MoveWindow(hwndForward_, 74, y, 68, h, TRUE);
            MoveWindow(hwndReload_, 146, y, 70, h, TRUE);
            MoveWindow(hwndHome_, 220, y, 56, h, TRUE);
            MoveWindow(hwndNewTab_, 280, y, 40, h, TRUE);
            MoveWindow(hwndSearchEngine_, 326, y, 110, h, TRUE);
            MoveWindow(hwndSettings_, 442, y, 75, h, TRUE);
            MoveWindow(hwndTime_, 523, y, 120, h, TRUE);
            addressX = 649;
        } else {
            MoveWindow(hwndBack_, 8, y, 34, h, TRUE);
            MoveWindow(hwndForward_, 46, y, 34, h, TRUE);
            MoveWindow(hwndReload_, 84, y, 34, h, TRUE);
            MoveWindow(hwndHome_, 122, y, 40, h, TRUE);
            MoveWindow(hwndNewTab_, 166, y, 58, h, TRUE);
            MoveWindow(hwndSearchEngine_, 232, y, 185, h, TRUE);
            MoveWindow(hwndSettings_, 421, y, 75, h, TRUE);
            MoveWindow(hwndTime_, 502, y, 120, h, TRUE);
            addressX = 628;
        }

        const int goX = std::max(addressX + 120, width - 84);
        const int addressWidth = std::max(80, goX - addressX - 8);
        MoveWindow(hwndAddress_, addressX, y, addressWidth, h, TRUE);
        MoveWindow(hwndGo_, goX, y, 76, h, TRUE);
    }

    void ApplyTabVisibility() {
        RECT content = ContentRect();
        for (size_t i = 0; i < tabs_.size(); ++i) {
            Tab* tab = tabs_[i].get();
            if (!tab->controller) {
                continue;
            }
            tab->controller->put_IsVisible(i == activeTab_ ? TRUE : FALSE);
            tab->controller->put_Bounds(content);
            tab->controller->NotifyParentWindowPositionChanged();
        }
    }

    void ComputeTabRects(std::vector<RECT>& rects) {
        rects.clear();
        if (tabs_.empty() || !hwnd_) {
            return;
        }

        RECT client{};
        GetClientRect(hwnd_, &client);
        const int available = std::max(0, static_cast<int>(client.right));
        const int count = static_cast<int>(tabs_.size());

        std::vector<int> widths;
        widths.reserve(tabs_.size());
        int total = 0;
        for (const auto& tab : tabs_) {
            int width = 84 + static_cast<int>(tab->title.size()) * 8;
            width = std::clamp(width, 96, 190);
            widths.push_back(width);
            total += width;
        }

        int x = 0;
        for (int i = 0; i < count; ++i) {
            int width = widths[static_cast<size_t>(i)];
            if (total > available && available > 0) {
                width = std::max(72, available / count);
            }
            rects.push_back(RECT{x, 0, x + width, kTabHeight});
            x += width;
        }
    }

    int HitTestTab(int x, int y, bool& close) {
        close = false;
        if (y < 0 || y >= kTabHeight) {
            return -1;
        }
        std::vector<RECT> rects;
        ComputeTabRects(rects);
        for (size_t i = 0; i < rects.size(); ++i) {
            if (!PtInRect(&rects[i], POINT{x, y})) {
                continue;
            }
            const int tabWidth = rects[i].right - rects[i].left;
            if (tabWidth > 80 && x >= rects[i].right - 24) {
                close = true;
            }
            return static_cast<int>(i);
        }
        return -1;
    }

    void Paint() {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd_, &ps);
        RECT client{};
        GetClientRect(hwnd_, &client);
        
        bool darkMode = IsSystemDarkMode();
        COLORREF toolbarColor = GetToolbarColor();
        COLORREF bgColor = CalculateBackgroundColor(toolbarColor, darkMode);
        HBRUSH bgBrush = CreateSolidBrush(bgColor);
        
        FillRect(hdc, &client, bgBrush);
        DeleteObject(bgBrush);
        
        DrawTabs(hdc);
        EndPaint(hwnd_, &ps);
    }

    void DrawTabs(HDC hdc) {
        std::vector<RECT> rects;
        ComputeTabRects(rects);
        SetBkMode(hdc, TRANSPARENT);

        const bool ie = IsIeMode();
        HBRUSH ieActiveBrush = ie ? CreateSolidBrush(RGB(255, 255, 255)) : nullptr;
        HBRUSH ieInactiveBrush = ie ? CreateSolidBrush(RGB(238, 238, 238)) : nullptr;
        const COLORREF ieBorder = RGB(172, 172, 172);

        for (size_t i = 0; i < rects.size(); ++i) {
            const bool active = i == activeTab_;
            HBRUSH brush = ie ? (active ? ieActiveBrush : ieInactiveBrush)
                              : GetSysColorBrush(active ? COLOR_WINDOW : COLOR_BTNFACE);
            FillRect(hdc, &rects[i], brush);

            const COLORREF borderColor = ie ? ieBorder : GetSysColor(active ? COLOR_WINDOWTEXT : COLOR_BTNSHADOW);
            HPEN pen = CreatePen(PS_SOLID, 1, borderColor);
            HGDIOBJ oldPen = SelectObject(hdc, pen);
            HGDIOBJ oldBrush = SelectObject(hdc, GetStockObject(NULL_BRUSH));
            Rectangle(hdc, rects[i].left, rects[i].top, rects[i].right, rects[i].bottom);
            SelectObject(hdc, oldBrush);
            SelectObject(hdc, oldPen);
            DeleteObject(pen);

            RECT textRect = rects[i];
            textRect.left += 10;
            textRect.right -= 28;
            std::wstring title = tabs_[i]->title.empty() ? L"New Tab" : tabs_[i]->title;
            DrawTextW(hdc, title.c_str(), -1, &textRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

            RECT closeRect{rects[i].right - 22, rects[i].top + 9, rects[i].right - 6, rects[i].top + 25};
            HBRUSH closeBrush = ie ? (active ? ieActiveBrush : ieInactiveBrush)
                                   : GetSysColorBrush(active ? COLOR_WINDOW : COLOR_BTNFACE);
            FillRect(hdc, &closeRect, closeBrush);
            DrawTextW(hdc, L"x", -1, &closeRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        }

        if (ieActiveBrush) DeleteObject(ieActiveBrush);
        if (ieInactiveBrush) DeleteObject(ieInactiveBrush);
    }

    std::wstring GetEditText(HWND edit) {
        int length = GetWindowTextLengthW(edit);
        if (length <= 0) {
            return {};
        }
        std::wstring text(static_cast<size_t>(length) + 1, L'\0');
        GetWindowTextW(edit, text.data(), length + 1);
        text.resize(static_cast<size_t>(length));
        return text;
    }

    static std::wstring Trim(const std::wstring& value) {
        const size_t first = value.find_first_not_of(L" \t\r\n");
        if (first == std::wstring::npos) {
            return {};
        }
        const size_t last = value.find_last_not_of(L" \t\r\n");
        return value.substr(first, last - first + 1);
    }

    static bool StartsWith(const std::wstring& value, const std::wstring& prefix) {
        return value.size() >= prefix.size() && value.compare(0, prefix.size(), prefix) == 0;
    }

    static std::string UrlEncodeUtf8(const std::wstring& value) {
        int utf8Length = WideCharToMultiByte(CP_UTF8, 0, value.c_str(), -1, nullptr, 0, nullptr, nullptr);
        if (utf8Length <= 1) {
            return {};
        }
        std::string utf8(static_cast<size_t>(utf8Length), '\0');
        WideCharToMultiByte(CP_UTF8, 0, value.c_str(), -1, utf8.data(), utf8Length, nullptr, nullptr);
        utf8.pop_back();

        std::string encoded;
        encoded.reserve(utf8.size() * 3);
        auto isUnreserved = [](unsigned char c) {
            return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
                   c == '-' || c == '_' || c == '.' || c == '~';
        };
        for (unsigned char c : utf8) {
            if (isUnreserved(c)) {
                encoded.push_back(static_cast<char>(c));
            } else {
                char buffer[4];
                snprintf(buffer, sizeof(buffer), "%%%02X", c);
                encoded += buffer;
            }
        }
        return encoded;
    }

    std::wstring BuildSearchUrl(const std::string& encodedQuery) {
        std::wstring encoded(encodedQuery.begin(), encodedQuery.end());
        if (searchEngine_ == SearchEngine::Searxng) {
            std::wstring base = searxngUrl_.empty() ? L"https://searx.be" : searxngUrl_;
            if (base.back() != L'/') {
                base += L'/';
            }
            return base + L"search?q=" + encoded;
        }
        if (IsIeMode()) {
            std::wstring decoded;
            for (size_t i = 0; i < encoded.size(); ++i) {
                if (encoded[i] == '%' && i + 2 < encoded.size()) {
                    int hi = encoded[i+1], lo = encoded[i+2];
                    int val = (hi <= '9' ? hi - '0' : hi <= 'F' ? hi - 'A' + 10 : hi - 'a' + 10) * 16 +
                              (lo <= '9' ? lo - '0' : lo <= 'F' ? lo - 'A' + 10 : lo - 'a' + 10);
                    decoded += static_cast<wchar_t>(val);
                    i += 2;
                } else {
                    decoded += static_cast<wchar_t>(encoded[i]);
                }
            }
            EnsureIeResultsPage(decoded);
            return IeResultsPageUrl();
        }
        if (searchEngine_ == SearchEngine::Excite ||
            searchEngine_ == SearchEngine::Infoseek ||
            searchEngine_ == SearchEngine::Lycos) {
            std::wstring decoded;
            for (size_t i = 0; i < encoded.size(); ++i) {
                if (encoded[i] == '%' && i + 2 < encoded.size()) {
                    int hi = encoded[i+1], lo = encoded[i+2];
                    int val = (hi <= '9' ? hi - '0' : hi <= 'F' ? hi - 'A' + 10 : hi - 'a' + 10) * 16 +
                              (lo <= '9' ? lo - '0' : lo <= 'F' ? lo - 'A' + 10 : lo - 'a' + 10);
                    decoded += static_cast<wchar_t>(val);
                    i += 2;
                } else {
                    decoded += static_cast<wchar_t>(encoded[i]);
                }
            }
            EnsureEngineResultsPage(searchEngine_, decoded);
            return EngineResultsPageUrl(searchEngine_);
        }
        const SearchEngineDef& def = SearchEngineDefFor(searchEngine_);
        if (def.searchUrlPrefix == nullptr) {
            return def.homeUrl ? std::wstring(def.homeUrl) : L"https://www.bing.com";
        }
        return std::wstring(def.searchUrlPrefix) + encoded;
    }

    std::wstring NormalizeUrl(const std::wstring& input) {
        std::wstring url = Trim(input);
        if (url.empty()) {
            return homeUrl_;
        }
        if (StartsWith(url, L"http://") || StartsWith(url, L"https://") || StartsWith(url, L"about:") ||
            StartsWith(url, L"file:") || StartsWith(url, L"data:") || StartsWith(url, L"chrome:")) {
            return url;
        }
        if (StartsWith(url, L"//")) {
            return L"https:" + url;
        }

        bool looksLikeHost = url.find(L'.') != std::wstring::npos ||
                             url.find(L':') != std::wstring::npos ||
                             StartsWith(url, L"localhost");
        if (looksLikeHost) {
            return L"https://" + url;
        }

        return BuildSearchUrl(UrlEncodeUtf8(url));
    }

    void ShowSearchEngineMenu() {
        POINT point{};
        GetCursorPos(&point);
        HMENU menu = CreatePopupMenu();
        if (!menu) {
            return;
        }

        int current = static_cast<int>(searchEngine_);
        for (int i = 0; i < kSearchEngineCount; ++i) {
            SearchEngine engine = static_cast<SearchEngine>(i);
            UINT flags = MF_STRING | (i == current ? MF_CHECKED : 0);
            AppendMenuW(menu, flags, kSearchEngineMenuBase + i, SearchEngineDisplayName(engine));
        }

        int command = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON | TPM_NONOTIFY,
                                     point.x, point.y, 0, hwnd_, nullptr);
        DestroyMenu(menu);
        if (command >= kSearchEngineMenuBase && command < kSearchEngineMenuBase + kSearchEngineCount) {
            SetSearchEngine(static_cast<SearchEngine>(command - kSearchEngineMenuBase));
        }
    }

    void ShowSettingsMenu() {
        POINT point{};
        GetCursorPos(&point);
        HMENU menu = CreatePopupMenu();
        if (!menu) {
            return;
        }

        HMENU themeMenu = CreatePopupMenu();
        AppendMenuW(themeMenu, MF_STRING | (darkMode_ ? 0 : MF_CHECKED), ID_SETTINGS_THEME_LIGHT, L"Light Mode");
        AppendMenuW(themeMenu, MF_STRING | (darkMode_ ? MF_CHECKED : 0), ID_SETTINGS_THEME_DARK, L"Dark Mode");
        AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(themeMenu), L"Theme");

        HMENU timezoneMenu = CreatePopupMenu();
        const wchar_t* timezones[] = {
            L"UTC", L"America/New_York", L"America/Los_Angeles", L"America/Chicago",
            L"Europe/London", L"Europe/Paris", L"Europe/Berlin", L"Asia/Tokyo",
            L"Asia/Shanghai", L"Asia/Kolkata", L"Australia/Sydney", L"Pacific/Auckland"
        };
        for (size_t i = 0; i < sizeof(timezones)/sizeof(timezones[0]); ++i) {
            UINT flags = MF_STRING;
            if (timezone_ == timezones[i]) flags |= MF_CHECKED;
            AppendMenuW(timezoneMenu, flags, kTimezoneMenuBase + i, timezones[i]);
        }
        AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(timezoneMenu), L"Timezone");

        AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(menu, MF_STRING, ID_SETTINGS_CLEAR_DATA, L"Clear Data");
        AppendMenuW(menu, MF_STRING, ID_SETTINGS_CLEAR_HISTORY, L"Clear History");

        int command = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON | TPM_NONOTIFY,
                                     point.x, point.y, 0, hwnd_, nullptr);
        DestroyMenu(menu);

        if (command == ID_SETTINGS_CLEAR_DATA) {
            ClearUserData();
        } else if (command == ID_SETTINGS_CLEAR_HISTORY) {
            ClearSearchHistory();
        } else if (command == ID_SETTINGS_THEME_LIGHT) {
            darkMode_ = false;
            SaveSettings();
            ApplyTheme();
        } else if (command == ID_SETTINGS_THEME_DARK) {
            darkMode_ = true;
            SaveSettings();
            ApplyTheme();
        } else if (command >= kTimezoneMenuBase && command < kTimezoneMenuBase + sizeof(timezones)/sizeof(timezones[0])) {
            timezone_ = timezones[command - kTimezoneMenuBase];
            SaveSettings();
            UpdateTimeDisplay();
        }
    }

    bool IsIeMode() const {
        return searchEngine_ == SearchEngine::InternetExplorer;
    }

    std::wstring IeStartPagePath() const {
        std::wstring settings = GetSettingsPath();
        const size_t slash = settings.find_last_of(L"\\");
        const std::wstring dir = slash == std::wstring::npos ? L"." : settings.substr(0, slash);
        return dir + L"\\ie_start.html";
    }

    std::wstring IeStartPageUrl() const {
        std::wstring path = IeStartPagePath();
        std::replace(path.begin(), path.end(), L'\\', L'/');
        return L"file:///" + path;
    }

    std::wstring IeResultsPagePath() const {
        std::wstring settings = GetSettingsPath();
        const size_t slash = settings.find_last_of(L"\\");
        const std::wstring dir = slash == std::wstring::npos ? L"." : settings.substr(0, slash);
        return dir + L"\\ie_results.html";
    }

    std::wstring IeResultsPageUrl() const {
        std::wstring path = IeResultsPagePath();
        std::replace(path.begin(), path.end(), L'\\', L'/');
        return L"file:///" + path;
    }

    std::wstring EngineResultsPagePath(SearchEngine engine) const {
        std::wstring settings = GetSettingsPath();
        const size_t slash = settings.find_last_of(L"\\");
        const std::wstring dir = slash == std::wstring::npos ? L"." : settings.substr(0, slash);
        std::wstring name;
        if (engine == SearchEngine::Excite) name = L"excite_results.html";
        else if (engine == SearchEngine::Infoseek) name = L"infoseek_results.html";
        else if (engine == SearchEngine::Lycos) name = L"lycos_results.html";
        else name = L"results.html";
        return dir + L"\\" + name;
    }

    std::wstring EngineResultsPageUrl(SearchEngine engine) const {
        std::wstring path = EngineResultsPagePath(engine);
        std::replace(path.begin(), path.end(), L'\\', L'/');
        return L"file:///" + path;
    }

    void EnsureEngineResultsPage(SearchEngine engine, const std::wstring& query) {
        const std::wstring path = EngineResultsPagePath(engine);
        const size_t slash = path.find_last_of(L"\\");
        if (slash != std::wstring::npos) {
            CreateDirectoryW(path.substr(0, slash).c_str(), nullptr);
        }
        std::ofstream file{std::filesystem::path(path), std::ios::out | std::ios::trunc | std::ios::binary};
        if (file) {
            if (engine == SearchEngine::Excite) file << BuildExciteResultsPageHtml(query);
            else if (engine == SearchEngine::Infoseek) file << BuildInfoseekResultsPageHtml(query);
            else if (engine == SearchEngine::Lycos) file << BuildLycosResultsPageHtml(query);
        }
    }

    void EnsureIeStartPage() {
        const std::wstring path = IeStartPagePath();
        const size_t slash = path.find_last_of(L"\\");
        if (slash != std::wstring::npos) {
            CreateDirectoryW(path.substr(0, slash).c_str(), nullptr);
        }
        std::ofstream file{std::filesystem::path(path), std::ios::out | std::ios::trunc | std::ios::binary};
        if (file) {
            file << BuildIeStartPageHtml();
        }
    }

    std::wstring EngineStartPagePath(SearchEngine engine) const {
        std::wstring settings = GetSettingsPath();
        const size_t slash = settings.find_last_of(L"\\");
        const std::wstring dir = slash == std::wstring::npos ? L"." : settings.substr(0, slash);
        std::wstring name;
        if (engine == SearchEngine::Excite) name = L"excite_start.html";
        else if (engine == SearchEngine::Infoseek) name = L"infoseek_start.html";
        else if (engine == SearchEngine::Lycos) name = L"lycos_start.html";
        else if (engine == SearchEngine::AltaVista) name = L"altavista_start.html";
        else name = L"start.html";
        return dir + L"\\" + name;
    }

    std::wstring EngineStartPageUrl(SearchEngine engine) const {
        std::wstring path = EngineStartPagePath(engine);
        std::replace(path.begin(), path.end(), L'\\', L'/');
        return L"file:///" + path;
    }

    void EnsureEngineStartPage(SearchEngine engine) {
        const std::wstring path = EngineStartPagePath(engine);
        const size_t slash = path.find_last_of(L"\\");
        if (slash != std::wstring::npos) {
            CreateDirectoryW(path.substr(0, slash).c_str(), nullptr);
        }
        std::ofstream file{std::filesystem::path(path), std::ios::out | std::ios::trunc | std::ios::binary};
        if (file) {
            if (engine == SearchEngine::Excite) file << BuildExciteStartPageHtml();
            else if (engine == SearchEngine::Infoseek) file << BuildInfoseekStartPageHtml();
            else if (engine == SearchEngine::Lycos) file << BuildLycosStartPageHtml();
            else if (engine == SearchEngine::AltaVista) file << BuildAltaVistaStartPageHtml();
        }
    }

    void EnsureIeResultsPage(const std::wstring& query) {
        const std::wstring path = IeResultsPagePath();
        const size_t slash = path.find_last_of(L"\\");
        if (slash != std::wstring::npos) {
            CreateDirectoryW(path.substr(0, slash).c_str(), nullptr);
        }
        std::ofstream file{std::filesystem::path(path), std::ios::out | std::ios::trunc | std::ios::binary};
        if (file) {
            file << BuildIeResultsPageHtml(query);
        }
    }

    void ApplySearchEngine() {
        const SearchEngineDef& def = SearchEngineDefFor(searchEngine_);
        if (searchEngine_ == SearchEngine::Searxng) {
            homeUrl_ = searxngUrl_.empty() ? L"https://searx.be" : searxngUrl_;
        } else if (IsIeMode()) {
            EnsureIeStartPage();
            homeUrl_ = IeStartPageUrl();
        } else if (searchEngine_ == SearchEngine::Excite ||
                   searchEngine_ == SearchEngine::Infoseek ||
                   searchEngine_ == SearchEngine::Lycos ||
                   searchEngine_ == SearchEngine::AltaVista) {
            EnsureEngineStartPage(searchEngine_);
            homeUrl_ = EngineStartPageUrl(searchEngine_);
        } else if (def.homeUrl) {
            homeUrl_ = std::wstring(def.homeUrl);
        } else {
            homeUrl_ = L"https://www.bing.com";
        }
        ApplyChromeLabels();
        UpdateSearchEngineButton();
    }

    void ApplyIeMode() {
        EnsureIeStartPage();
        homeUrl_ = IeStartPageUrl();
        ApplyChromeLabels();
        UpdateSearchEngineButton();
    }

    HICON CreateFallbackIcon(SearchEngine engine) {
        HDC hdc = GetDC(nullptr);
        if (!hdc) return nullptr;

        int size = 32;
        HDC memDC = CreateCompatibleDC(hdc);
        HBITMAP bitmap = CreateCompatibleBitmap(hdc, size, size);
        HBITMAP oldBitmap = (HBITMAP)SelectObject(memDC, bitmap);

        RECT rect = {0, 0, size, size};
        COLORREF bgColor = darkMode_ ? RGB(30, 30, 30) : RGB(255, 255, 255);
        FillRect(memDC, &rect, CreateSolidBrush(bgColor));

        COLORREF primaryColor = RGB(0, 0, 255);
        wchar_t letter = L'G';

        switch (engine) {
            case SearchEngine::Google: primaryColor = RGB(66, 133, 244); letter = L'G'; break;
            case SearchEngine::Bing: primaryColor = RGB(0, 164, 239); letter = L'b'; break;
            case SearchEngine::DuckDuckGo: primaryColor = RGB(212, 88, 14); letter = L'D'; break;
            case SearchEngine::Yahoo: primaryColor = RGB(131, 20, 140); letter = L'Y'; break;
            case SearchEngine::Yandex: primaryColor = RGB(255, 204, 0); letter = L'Y'; break;
            case SearchEngine::Ecosia: primaryColor = RGB(73, 140, 73); letter = L'E'; break;
            case SearchEngine::AOL: primaryColor = RGB(0, 102, 204); letter = L'A'; break;
            case SearchEngine::InternetArchive: primaryColor = RGB(100, 100, 100); letter = L'I'; break;
            case SearchEngine::Searxng: primaryColor = RGB(128, 0, 128); letter = L'S'; break;
            case SearchEngine::InternetExplorer: primaryColor = RGB(0, 114, 198); letter = L'e'; break;
            case SearchEngine::Excite: primaryColor = RGB(255, 204, 0); letter = L'E'; break;
            case SearchEngine::Infoseek: primaryColor = RGB(255, 255, 0); letter = L'I'; break;
            case SearchEngine::WebCrawler: primaryColor = RGB(0, 102, 204); letter = L'W'; break;
            case SearchEngine::Lycos: primaryColor = RGB(0, 102, 204); letter = L'L'; break;
            case SearchEngine::Dogpile: primaryColor = RGB(255, 102, 0); letter = L'D'; break;
            case SearchEngine::AltaVista: primaryColor = RGB(0, 0, 128); letter = L'A'; break;
            case SearchEngine::Mojeek: primaryColor = RGB(0, 153, 0); letter = L'M'; break;
            case SearchEngine::StartPage: primaryColor = RGB(255, 140, 0); letter = L'S'; break;
            case SearchEngine::WolframAlpha: primaryColor = RGB(220, 20, 60); letter = L'W'; break;
            case SearchEngine::MetaGer: primaryColor = RGB(0, 128, 0); letter = L'M'; break;
            case SearchEngine::Swisscows: primaryColor = RGB(128, 0, 128); letter = L'S'; break;
            case SearchEngine::Qwant: primaryColor = RGB(0, 102, 204); letter = L'Q'; break;
            case SearchEngine::Seznam: primaryColor = RGB(0, 51, 153); letter = L'S'; break;
        }

        HBRUSH brush = CreateSolidBrush(primaryColor);
        HPEN pen = CreatePen(PS_SOLID, 2, primaryColor);
        SelectObject(memDC, brush);
        SelectObject(memDC, pen);

        Ellipse(memDC, 2, 2, size-2, size-2);

        HFONT font = CreateFontW(20, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Arial");
        HFONT oldFont = (HFONT)SelectObject(memDC, font);
        SetTextColor(memDC, RGB(255, 255, 255));
        SetBkMode(memDC, TRANSPARENT);

        wchar_t text[2] = {letter, 0};
        DrawTextW(memDC, text, -1, &rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        SelectObject(memDC, oldFont);
        SelectObject(memDC, oldBitmap);
        DeleteObject(font);
        DeleteObject(brush);
        DeleteObject(pen);
        DeleteDC(memDC);
        ReleaseDC(nullptr, hdc);

        ICONINFO ii = {TRUE, 0, 0, bitmap, bitmap};
        HICON icon = CreateIconIndirect(&ii);
        DeleteObject(bitmap);

        return icon;
    }

    HICON CreateSearchEngineIcon(SearchEngine engine) {
        std::wstring iconPath = L"icons\\";
        switch (engine) {
            case SearchEngine::Google: iconPath += L"google.ico"; break;
            case SearchEngine::Bing: iconPath += L"bing.ico"; break;
            case SearchEngine::DuckDuckGo: iconPath += L"duckduckgo.ico"; break;
            case SearchEngine::Yahoo: iconPath += L"yahoo.ico"; break;
            case SearchEngine::Yandex: iconPath += L"yandex.ico"; break;
            case SearchEngine::Ecosia: iconPath += L"ecosia.ico"; break;
            case SearchEngine::AOL: iconPath += L"aol.ico"; break;
            case SearchEngine::InternetArchive: iconPath += L"internetarchive.ico"; break;
            case SearchEngine::Searxng: iconPath += L"searxng.ico"; break;
            case SearchEngine::InternetExplorer: iconPath += L"internetexplorer.ico"; break;
            case SearchEngine::Excite: iconPath += L"excite.ico"; break;
            case SearchEngine::Infoseek: iconPath += L"infoseek.ico"; break;
            case SearchEngine::WebCrawler: iconPath += L"webcrawler.ico"; break;
            case SearchEngine::Lycos: iconPath += L"lycos.ico"; break;
            case SearchEngine::Dogpile: iconPath += L"dogpile.ico"; break;
            case SearchEngine::AltaVista: iconPath += L"altavista.ico"; break;
            case SearchEngine::Mojeek: iconPath += L"mojeek.ico"; break;
            case SearchEngine::StartPage: iconPath += L"startpage.ico"; break;
            case SearchEngine::WolframAlpha: iconPath += L"wolframalpha.ico"; break;
            case SearchEngine::MetaGer: iconPath += L"metager.ico"; break;
            case SearchEngine::Swisscows: iconPath += L"swisscows.ico"; break;
            case SearchEngine::Qwant: iconPath += L"qwant.ico"; break;
            case SearchEngine::Seznam: iconPath += L"seznam.ico"; break;
        }

        HICON icon = (HICON)LoadImageW(nullptr, iconPath.c_str(), IMAGE_ICON, 32, 32, LR_LOADFROMFILE);
        if (icon) {
            return icon;
        }

        return CreateFallbackIcon(engine);
    }

    void ApplyChromeLabels() {
        const bool ie = IsIeMode();
        if (hwndBack_) SetWindowTextW(hwndBack_, ie ? L"Back" : L"<");
        if (hwndForward_) SetWindowTextW(hwndForward_, ie ? L"Forward" : L">");
        if (hwndReload_) SetWindowTextW(hwndReload_, ie ? L"Refresh" : L"R");
        if (hwndHome_) SetWindowTextW(hwndHome_, ie ? L"Home" : L"H");
        if (hwnd_) {
            HICON icon = CreateSearchEngineIcon(searchEngine_);
            SendMessageW(hwnd_, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(icon));
            SendMessageW(hwnd_, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(icon));
            LayoutControls();
        }
        InvalidateRect(hwnd_, nullptr, FALSE);
    }

    void ApplyTheme() {
        COLORREF bgColor = darkMode_ ? RGB(30, 30, 30) : RGB(255, 255, 255);
        HBRUSH brush = CreateSolidBrush(bgColor);
        SetClassLongPtrW(hwnd_, GCLP_HBRBACKGROUND, reinterpret_cast<LONG_PTR>(brush));
        InvalidateRect(hwnd_, nullptr, TRUE);
    }

    void UpdateTimeDisplay() {
        if (!hwndTime_) return;

        time_t now = time(nullptr);
        struct tm* timeinfo = gmtime(&now);
        if (!timeinfo) return;

        int offset = 0;
        if (timezone_ == L"UTC") offset = 0;
        else if (timezone_ == L"America/New_York") offset = -5;
        else if (timezone_ == L"America/Los_Angeles") offset = -8;
        else if (timezone_ == L"America/Chicago") offset = -6;
        else if (timezone_ == L"Europe/London") offset = 0;
        else if (timezone_ == L"Europe/Paris") offset = 1;
        else if (timezone_ == L"Europe/Berlin") offset = 1;
        else if (timezone_ == L"Asia/Tokyo") offset = 9;
        else if (timezone_ == L"Asia/Shanghai") offset = 8;
        else if (timezone_ == L"Asia/Kolkata") offset = 5;
        else if (timezone_ == L"Australia/Sydney") offset = 10;
        else if (timezone_ == L"Pacific/Auckland") offset = 12;

        int hour = timeinfo->tm_hour + offset;
        if (hour < 0) hour += 24;
        if (hour >= 24) hour -= 24;

        wchar_t buffer[64];
        swprintf_s(buffer, L"%02d:%02d:%02d %s", hour, timeinfo->tm_min, timeinfo->tm_sec, timezone_.c_str());
        SetWindowTextW(hwndTime_, buffer);
    }

    void UpdateWindowTitle() {
        if (!hwnd_) {
            return;
        }
        Tab* tab = ActiveTab();
        std::wstring title = tab && !tab->title.empty() ? tab->title : L"New Tab";
        SetWindowTextW(hwnd_, (title + (IsIeMode() ? L" - Internet Explorer" : L" - Custom Browser")).c_str());
    }

    void SetSearchEngine(SearchEngine engine) {
        searchEngine_ = engine;
        ApplySearchEngine();
        SaveSettings();
    }

    void UpdateSearchEngineButton() {
        if (!hwndSearchEngine_) {
            return;
        }
        std::wstring text = L"Search: ";
        if (IsIeMode()) {
            text += L"Google";
        } else {
            text += SearchEngineDisplayName(searchEngine_);
        }
        SetWindowTextW(hwndSearchEngine_, text.c_str());
    }

    std::wstring GetSettingsPath() const {
        wchar_t localAppData[MAX_PATH]{};
        if (!GetEnvironmentVariableW(L"LOCALAPPDATA", localAppData, MAX_PATH)) {
            return {};
        }
        return std::wstring(localAppData) + L"\\CustomBrowser\\settings.txt";
    }

    void LoadSettings() {
        std::wstring path = GetSettingsPath();
        if (path.empty()) {
            return;
        }

        std::ifstream file{std::filesystem::path(path)};
        if (!file) {
            ApplySearchEngine();
            return;
        }

        std::string line;
        while (std::getline(file, line)) {
            const size_t equals = line.find('=');
            if (equals == std::string::npos) {
                continue;
            }
            const std::string key = TrimAscii(line.substr(0, equals));
            const std::string value = TrimAscii(line.substr(equals + 1));
            if (key == "search_engine") {
                bool allDigits = !value.empty();
                int index = 0;
                for (char c : value) {
                    if (c < '0' || c > '9') {
                        allDigits = false;
                        break;
                    }
                    index = index * 10 + (c - '0');
                }
                if (allDigits) {
                    searchEngine_ = SearchEngineFromIndex(index);
                } else {
                    SearchEngine parsed = searchEngine_;
                    if (TryParseSearchEngineName(value, parsed)) {
                        searchEngine_ = parsed;
                    }
                }
            } else if (key == "searxng_url") {
                searxngUrl_ = std::wstring(value.begin(), value.end());
            } else if (key == "dark_mode") {
                darkMode_ = (value == "1");
            } else if (key == "timezone") {
                timezone_ = std::wstring(value.begin(), value.end());
            }
        }
        ApplySearchEngine();
        ApplyTheme();
        UpdateTimeDisplay();
    }

    void SaveSettings() {
        std::wstring path = GetSettingsPath();
        if (path.empty()) {
            return;
        }
        const size_t slash = path.find_last_of(L"\\");
        if (slash != std::wstring::npos) {
            CreateDirectoryW(path.substr(0, slash).c_str(), nullptr);
        }

        std::ofstream file{std::filesystem::path(path), std::ios::out | std::ios::trunc};
        if (!file) {
            return;
        }
        file << "search_engine=" << SearchEngineSettingName(searchEngine_) << "\n";
        file << "searxng_url=" << NarrowAscii(searxngUrl_) << "\n";
        file << "dark_mode=" << (darkMode_ ? "1" : "0") << "\n";
        file << "timezone=" << NarrowAscii(timezone_) << "\n";
    }
};

#define WV2_IUNKNOWN(ClassName, InterfaceType, IidConstant)                                  \
public:                                                                                     \
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) override {       \
        if (!ppvObject) {                                                                   \
            return E_POINTER;                                                               \
        }                                                                                   \
        if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IidConstant)) {               \
            *ppvObject = static_cast<InterfaceType*>(this);                                 \
            AddRef();                                                                       \
            return S_OK;                                                                    \
        }                                                                                   \
        *ppvObject = nullptr;                                                               \
        return E_NOINTERFACE;                                                               \
    }                                                                                       \
    ULONG STDMETHODCALLTYPE AddRef() override {                                             \
        return ++refCount_;                                                                 \
    }                                                                                       \
    ULONG STDMETHODCALLTYPE Release() override {                                            \
        ULONG count = --refCount_;                                                          \
        if (count == 0) {                                                                   \
            delete this;                                                                    \
        }                                                                                   \
        return count;                                                                       \
    }                                                                                       \
private:                                                                                    \
    ULONG refCount_ = 1;

class EnvironmentCreatedHandler final : public ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler {
public:
    explicit EnvironmentCreatedHandler(BrowserWindow* owner) : owner_(owner) {}

    WV2_IUNKNOWN(EnvironmentCreatedHandler, ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler,
                 IID_ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler)

    HRESULT STDMETHODCALLTYPE Invoke(HRESULT errorCode, ICoreWebView2Environment* result) override {
        if (owner_) {
            owner_->OnEnvironmentCreated(errorCode, result);
        }
        return S_OK;
    }

private:
    BrowserWindow* owner_;
};

class ControllerCreatedHandler final : public ICoreWebView2CreateCoreWebView2ControllerCompletedHandler {
public:
    ControllerCreatedHandler(BrowserWindow* owner, int tabId) : owner_(owner), tabId_(tabId) {}

    WV2_IUNKNOWN(ControllerCreatedHandler, ICoreWebView2CreateCoreWebView2ControllerCompletedHandler,
                 IID_ICoreWebView2CreateCoreWebView2ControllerCompletedHandler)

    HRESULT STDMETHODCALLTYPE Invoke(HRESULT errorCode, ICoreWebView2Controller* result) override {
        if (owner_) {
            owner_->OnControllerCreated(tabId_, errorCode, result);
        }
        return S_OK;
    }

private:
    BrowserWindow* owner_;
    int tabId_;
};

class NavigationStartingHandler final : public ICoreWebView2NavigationStartingEventHandler {
public:
    NavigationStartingHandler(BrowserWindow* owner, int tabId) : owner_(owner), tabId_(tabId) {}

    WV2_IUNKNOWN(NavigationStartingHandler, ICoreWebView2NavigationStartingEventHandler,
                 IID_ICoreWebView2NavigationStartingEventHandler)

    HRESULT STDMETHODCALLTYPE Invoke(ICoreWebView2* sender, ICoreWebView2NavigationStartingEventArgs* args) override {
        if (owner_) {
            owner_->OnNavigationStarting(tabId_, sender, args);
        }
        return S_OK;
    }

private:
    BrowserWindow* owner_;
    int tabId_;
};

class NavigationCompletedHandler final : public ICoreWebView2NavigationCompletedEventHandler {
public:
    NavigationCompletedHandler(BrowserWindow* owner, int tabId) : owner_(owner), tabId_(tabId) {}

    WV2_IUNKNOWN(NavigationCompletedHandler, ICoreWebView2NavigationCompletedEventHandler,
                 IID_ICoreWebView2NavigationCompletedEventHandler)

    HRESULT STDMETHODCALLTYPE Invoke(ICoreWebView2* sender, ICoreWebView2NavigationCompletedEventArgs* args) override {
        if (owner_) {
            owner_->OnNavigationCompleted(tabId_, sender, args);
        }
        return S_OK;
    }

private:
    BrowserWindow* owner_;
    int tabId_;
};

class SourceChangedHandler final : public ICoreWebView2SourceChangedEventHandler {
public:
    SourceChangedHandler(BrowserWindow* owner, int tabId) : owner_(owner), tabId_(tabId) {}

    WV2_IUNKNOWN(SourceChangedHandler, ICoreWebView2SourceChangedEventHandler,
                 IID_ICoreWebView2SourceChangedEventHandler)

    HRESULT STDMETHODCALLTYPE Invoke(ICoreWebView2* sender, ICoreWebView2SourceChangedEventArgs* args) override {
        if (owner_) {
            owner_->OnSourceChanged(tabId_, sender, args);
        }
        return S_OK;
    }

private:
    BrowserWindow* owner_;
    int tabId_;
};

class DocumentTitleChangedHandler final : public ICoreWebView2DocumentTitleChangedEventHandler {
public:
    DocumentTitleChangedHandler(BrowserWindow* owner, int tabId) : owner_(owner), tabId_(tabId) {}

    WV2_IUNKNOWN(DocumentTitleChangedHandler, ICoreWebView2DocumentTitleChangedEventHandler,
                 IID_ICoreWebView2DocumentTitleChangedEventHandler)

    HRESULT STDMETHODCALLTYPE Invoke(ICoreWebView2* sender, IUnknown* args) override {
        if (owner_) {
            owner_->OnDocumentTitleChanged(tabId_, sender, args);
        }
        return S_OK;
    }

private:
    BrowserWindow* owner_;
    int tabId_;
};

class HistoryChangedHandler final : public ICoreWebView2HistoryChangedEventHandler {
public:
    HistoryChangedHandler(BrowserWindow* owner, int tabId) : owner_(owner), tabId_(tabId) {}

    WV2_IUNKNOWN(HistoryChangedHandler, ICoreWebView2HistoryChangedEventHandler,
                 IID_ICoreWebView2HistoryChangedEventHandler)

    HRESULT STDMETHODCALLTYPE Invoke(ICoreWebView2* sender, IUnknown* args) override {
        if (owner_) {
            owner_->OnHistoryChanged(tabId_, sender, args);
        }
        return S_OK;
    }

private:
    BrowserWindow* owner_;
    int tabId_;
};

class NewWindowRequestedHandler final : public ICoreWebView2NewWindowRequestedEventHandler {
public:
    NewWindowRequestedHandler(BrowserWindow* owner, int tabId) : owner_(owner), tabId_(tabId) {}

    WV2_IUNKNOWN(NewWindowRequestedHandler, ICoreWebView2NewWindowRequestedEventHandler,
                 IID_ICoreWebView2NewWindowRequestedEventHandler)

    HRESULT STDMETHODCALLTYPE Invoke(ICoreWebView2* sender, ICoreWebView2NewWindowRequestedEventArgs* args) override {
        (void)sender;
        if (owner_) {
            owner_->OnNewWindowRequested(tabId_, args);
        }
        return S_OK;
    }

private:
    BrowserWindow* owner_;
    int tabId_;
};

void BrowserWindow::InitWebView2() {
    wchar_t localAppData[MAX_PATH]{};
    if (!GetEnvironmentVariableW(L"LOCALAPPDATA", localAppData, MAX_PATH)) {
        MessageBoxW(hwnd_, L"Could not determine the local application data folder.", kWindowTitle, MB_ICONERROR | MB_OK);
        return;
    }

    std::wstring root = std::wstring(localAppData) + L"\\CustomBrowser";
    std::wstring userData = root + L"\\WebView2";
    CreateDirectoryW(root.c_str(), nullptr);
    CreateDirectoryW(userData.c_str(), nullptr);

    auto* handler = new EnvironmentCreatedHandler(this);
    HRESULT hr = CreateCoreWebView2EnvironmentWithOptions(nullptr, userData.c_str(), nullptr, handler);
    handler->Release();
    if (FAILED(hr)) {
        OnEnvironmentCreated(hr, nullptr);
    }
}

void BrowserWindow::EnsureController(Tab& tab) {
    if (!environmentReady_ || !environment_ || tab.controllerRequested || tab.controller) {
        return;
    }

    tab.controllerRequested = true;
    auto* handler = new ControllerCreatedHandler(this, tab.id);
    HRESULT hr = environment_->CreateCoreWebView2Controller(hwnd_, handler);
    handler->Release();
    if (FAILED(hr)) {
        OnControllerCreated(tab.id, hr, nullptr);
    }
}

void BrowserWindow::RegisterWebViewEvents(Tab& tab) {
    if (!tab.webView) {
        return;
    }

    auto* navigationStarting = new NavigationStartingHandler(this, tab.id);
    EventRegistrationToken token{};
    tab.webView->add_NavigationStarting(navigationStarting, &token);
    navigationStarting->Release();

    auto* navigationCompleted = new NavigationCompletedHandler(this, tab.id);
    tab.webView->add_NavigationCompleted(navigationCompleted, &token);
    navigationCompleted->Release();

    auto* sourceChanged = new SourceChangedHandler(this, tab.id);
    tab.webView->add_SourceChanged(sourceChanged, &token);
    sourceChanged->Release();

    auto* titleChanged = new DocumentTitleChangedHandler(this, tab.id);
    tab.webView->add_DocumentTitleChanged(titleChanged, &token);
    titleChanged->Release();

    auto* historyChanged = new HistoryChangedHandler(this, tab.id);
    tab.webView->add_HistoryChanged(historyChanged, &token);
    historyChanged->Release();

    auto* newWindow = new NewWindowRequestedHandler(this, tab.id);
    tab.webView->add_NewWindowRequested(newWindow, &token);
    newWindow->Release();
}

} // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int showCommand) {
    HRESULT comResult = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(comResult)) {
        return 1;
    }

    BrowserWindow* browser = new BrowserWindow(instance);
    if (!browser->Create(showCommand)) {
        delete browser;
        CoUninitialize();
        return 1;
    }

    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    delete browser;
    CoUninitialize();
    return static_cast<int>(message.wParam);
}
