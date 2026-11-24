# Custom Browser

A native Windows browser shell written in C++ with Win32 and Microsoft Edge WebView2.

This is a desktop browser application, not a from-scratch HTML rendering engine. WebView2 provides the web platform rendering, while this project provides the browser chrome: tabs, navigation controls, address bar, and tab lifecycle.

## Features

- Multiple tabs with a custom tab strip
- Back, forward, reload, home, new tab, and go controls
- Address bar with URL normalization and search fallback
- Search engine selector: Google, Bing, DuckDuckGo, Yahoo, Yandex, Ecosia, AOL, Internet Archive, SearXNG, Internet Explorer, Excite (1995), Infoseek (1994), Web Crawler (1994), Lycos (1994), Dogpile, AltaVista (1995), Mojeek, StartPage, WolframAlpha, MetaGer, Swisscows, Qwant, Seznam
- Classic 1990s search engines: Excite, Infoseek, and Lycos run locally with recreated 1990s interfaces and working search (powered by Google with era-styled results pages). Web Crawler links to the current webcrawler.com (now a metasearch engine). Start pages are generated dynamically in `%LOCALAPPDATA%\CustomBrowser\`.
- Internet Explorer option activates a standalone IE9-style UI recreation: simple IE9-era start page (logo, Favorites/Tools/Safety bar, era links, search bar), IE window title, IE toolbar labels/layout, IE-style icon, and Bing searches with a custom IE-era results page. The start page is written to `%LOCALAPPDATA%\CustomBrowser\ie_start.html`, results page to `ie_results.html`.
- Clear Data button (toolbar): deletes all user data including settings, IE start page, and WebView2 profile (cookies, localStorage, etc.). Resets search engine to Bing.
- Clear History button (toolbar): clears localStorage and sessionStorage for all open tabs.
- Per-tab navigation history state
- Opens `target=_blank`/new-window requests as new tabs
- Native C++ Win32 UI
- Custom browser icon (blue globe with "WorldWide Internet Search Engine" text)
- Dynamic background color based on Windows toolbar color and system dark/light mode setting

## Requirements

- Windows 10/11 x64
- Microsoft Edge WebView2 Runtime
- LLVM MinGW toolchain for building

Install the toolchain with:

```powershell
winget install MartinStorsjo.LLVM-MinGW.UCRT
```

The WebView2 SDK headers/libs are already vendored under `third_party/webview2`.

## Build

```powershell
powershell -ExecutionPolicy Bypass -File .\build.ps1
```

Output:

```text
build\CustomBrowser.exe
build\WebView2Loader.dll
build\browser_icon.png
```

## Run

```powershell
.\build\CustomBrowser.exe
```

Keep `WebView2Loader.dll` and `browser_icon.png` next to the executable.

## Notes

- User data is stored under `%LOCALAPPDATA%\CustomBrowser\WebView2`.
- Browser settings are stored in `%LOCALAPPDATA%\CustomBrowser\settings.txt`.
- The SearXNG instance URL can be changed with `searxng_url=` in `settings.txt`.
- The default home page is `https://www.bing.com`.
- `build.ps1` signs the exe with a local `CN=Custom Browser Dev` certificate and trusts it under `Cert:\CurrentUser\TrustedPublisher` so Smart App Control can launch it.
- If WebView2 Runtime is missing, the app shows an error message.
