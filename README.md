# Custom Browser

A native Windows browser shell written in C++ with Win32 and Microsoft Edge WebView2.

This is a desktop browser application, not a from-scratch HTML rendering engine. WebView2 provides the web platform rendering, while this project provides the browser chrome: tabs, navigation controls, address bar, and tab lifecycle.

## Features

- Multiple tabs with a custom tab strip
- Back, forward, reload, home, new tab, and go controls
- Address bar with URL normalization and search fallback
- Search engine selector: Google, Bing, DuckDuckGo, Yahoo, Yandex, Ecosia, AOL, Internet Archive, SearXNG, Internet Explorer, Excite (1995), Infoseek (1994), Web Crawler (1994), Lycos (1994), Dogpile, AltaVista (1995), Mojeek, StartPage, WolframAlpha, MetaGer, Swisscows, Qwant, Seznam
- Classic 1990s search engines: Excite, Infoseek, and Lycos run locally with recreated 1990s interfaces and working search (powered by Google). Web Crawler links to the current webcrawler.com (now a metasearch engine). Start pages are generated dynamically in `%LOCALAPPDATA%\CustomBrowser\`.
- Internet Explorer option activates a standalone IE9-style UI recreation: simple IE9-era start page (logo, Favorites/Tools/Safety bar, era links, search bar), IE window title, IE toolbar labels/layout, and IE-style icon. Searches go directly to Bing. The start page is written to `%LOCALAPPDATA%\CustomBrowser\ie_start.html`.
- Settings dropdown menu with theme (light/dark mode), timezone selection, Clear Data, and Clear History options
- Time display in toolbar showing current time in selected timezone (12 timezone options available)
- Per-tab navigation history state
- Opens `target=_blank`/new-window requests as new tabs
- Native C++ Win32 UI
- Dynamic icons per search engine (loads from icons folder with procedural fallback)
- Dynamic background color based on user-selected light/dark theme

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
```

## Run

```powershell
.\build\CustomBrowser.exe
```

Keep `WebView2Loader.dll` next to the executable. Search engine icons are loaded from the `icons/` folder in the project directory.

## Notes

- User data is stored under `%LOCALAPPDATA%\CustomBrowser\WebView2`.
- Browser settings are stored in `%LOCALAPPDATA%\CustomBrowser\settings.txt`.
- Settings include: search engine, dark mode (dark_mode=0/1), and timezone (timezone=...).
- The SearXNG instance URL can be changed with `searxng_url=` in `settings.txt`.
- The default home page is `https://www.bing.com`.
- Icon files for search engines are stored in the `icons/` folder; procedural fallback icons are used if files are missing.
- Code signing is currently disabled in the build script to avoid Smart App Control blocking.
