# Cinnamon Memos ToDo Applet

A Cinnamon applet that displays and interacts with ToDo items from a [UseMemos](https://usememos.com/) server.

## Features
- **Live Sync**: Fetches and displays tasks from a specific memo.
- **Interactive Checkboxes**: Toggle tasks directly from the panel popup.
- **Add Tasks**: Quickly add new items with one click.
- **Delete Tasks**: Remove entries using the trash icon.
- **Customizable**: Adjust font size, popup width, and panel width to suit your desktop.
- **Auto-Scrolling**: Cycles through memo lines on the panel label.

## Installation
1. Download or clone this repository.
2. Move the folder to `~/.local/share/cinnamon/applets/` (ensure the folder name matches the `uuid` in `metadata.json`).
3. Enable the applet via **Cinnamon System Settings** -> **Applets**.

## Configuration
Right-click the applet and select **Configure** to set:
- **Server URL**: The base URL of your Memos server.
- **Auth Token**: Your Memos API Refresh Token.
- **Memo ID**: The ID of the memo you wish to track.

## License
MIT
