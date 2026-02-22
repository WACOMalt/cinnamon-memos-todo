# Memos ToDo

Memos ToDo is a cross-platform suite of native applications designed to seamlessly synchronize and display Markdown-style to-do lists hosted on a self-hosted [UseMemos](https://usememos.com/) server.

This repository is split across three isolated branches, each containing a native implementation for a specific platform. 

## Platform Branches

Please switch to the relevant branch for the codebase and installation instructions of the platform you are interested in:

* 🐧 **[Linux (Cinnamon Applet)](https://github.com/WACOMalt/memos-todo/tree/linux-cinnamon)** 
    A native Javascript/Stjs applet built specifically for the Linux Mint Cinnamon desktop environment.
* 🪟 **[Windows (Taskbar Overlay)](https://github.com/WACOMalt/memos-todo/tree/windows)** 
    A lightweight, native C++ Win32 application that draws a transparent HUD directly above your taskbar.
* 📱 **[Android (Mobile App & Home Widget)](https://github.com/WACOMalt/memos-todo/tree/android)** 
    A native Kotlin / Jetpack Compose application featuring rich interactive home screen widgets.

## Features

Across all platforms, Memos ToDo implements a custom syntax parser that leverages UseMemos' general text fields to create interactive checklists:
* Uses `☐` and `☑` Unicode characters to represent checkbox states.
* Instantly updates the UseMemos backend via the API without requiring the UseMemos frontend.
* Follows OS-specific design languages (Adwaita on Linux/Windows, Material on Android).
