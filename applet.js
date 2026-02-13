/**
 * Memos ToDo Applet
 * Displays a specific memo from UseMemos on the Cinnamon panel.
 */

const Applet = imports.ui.applet;
const St = imports.gi.St;
const PopupMenu = imports.ui.popupMenu;
const Settings = imports.ui.settings;
const Mainloop = imports.mainloop;
const Lang = imports.lang;
const Soup = imports.gi.Soup;
const GLib = imports.gi.GLib;
const Gio = imports.gi.Gio; // Needed for browser launch
const Pango = imports.gi.Pango;
const Clutter = imports.gi.Clutter; // Needed for event handling in entry

class MemosApplet extends Applet.TextApplet {
    constructor(metadata, orientation, panel_height, instance_id) {
        super(orientation, panel_height, instance_id);

        this.set_applet_tooltip("Memos ToDo");
        this.set_applet_label("Loading...");

        // Initialize state variables
        this.memoLines = [];
        this.currentLineIndex = 0;
        this._scrollTimeoutId = null;
        this._dirty = false; // Track local changes
        this.items = []; // Initialize items array

        this.settings = new Settings.AppletSettings(this, metadata.uuid, instance_id);
        this._bindSettings();

        // Menu setup
        this.menuManager = new PopupMenu.PopupMenuManager(this);
        this.menu = new Applet.AppletPopupMenu(this, orientation);
        this.menuManager.addMenu(this.menu);

        this.menu.connect('open-state-changed', Lang.bind(this, (menu, open) => {
            if (!open) {
                this._hideAddEntry();
                if (this._dirty) {
                    this._saveChanges();
                }
            }
        }));

        // ScrollView for popup content
        this.scrollView = new St.ScrollView({
            hscrollbar_policy: St.PolicyType.NEVER,
            vscrollbar_policy: St.PolicyType.AUTOMATIC,
            style_class: 'memo-popup-scrollbox'
        });
        this.menu.addActor(this.scrollView);

        this.contentBox = new St.BoxLayout({ vertical: true, style_class: 'memo-popup-content' });
        this.contentLabel = new St.Label({ text: "Loading...", style_class: 'memo-popup-text' });

        this.contentBox.add(this.contentLabel);
        this.scrollView.add_actor(this.contentBox);

        // Apply initial styles from settings
        this._updateStyles();

        // Separator
        this.menu.addMenuItem(new PopupMenu.PopupSeparatorMenuItem());

        // Bottom Bar (Custom)
        this._buildBottomBar();

        // Soup session initialization
        if (Soup.MAJOR_VERSION === 2) {
            this._httpSession = new Soup.SessionAsync();
        } else {
            // Soup 3.0
            this._httpSession = new Soup.Session();
        }

        // Start loops
        this._updateLoop();
        this._startScrollLoop();
    }

    _buildBottomBar() {
        // Container Menu Item
        this.bottomBarItem = new PopupMenu.PopupBaseMenuItem({ reactive: true });

        // CRITICAL: Prevent the item from closing the menu when clicked
        this.bottomBarItem.activate = function (event) {
            return false;
        };

        let container = new St.BoxLayout({ vertical: true, x_expand: true });

        // 1. View Mode: [Open in Browser]   [+]
        this.bottomBarViewBox = new St.BoxLayout({
            vertical: false,
            style_class: 'memo-bottom-bar',
            x_expand: true
        });

        // Open Browser Link
        let browserBtn = new St.Button({
            style_class: 'memo-bottom-button',
            reactive: true,
            can_focus: true,
            x_expand: true
        });
        let browserLabel = new St.Label({ text: "Open in Browser" });
        browserBtn.set_child(browserLabel);
        browserBtn.connect('clicked', Lang.bind(this, this._openInBrowser));

        // Add Button (+)
        let addBtn = new St.Button({
            style_class: 'memo-bottom-button',
            reactive: true,
            can_focus: true,
            x_expand: false
        });
        let addLabel = new St.Label({ text: " + " });
        addBtn.set_child(addLabel);
        addBtn.connect('clicked', Lang.bind(this, this._showAddEntry));

        this.bottomBarViewBox.add(browserBtn, { expand: true, x_fill: true, y_fill: false });
        this.bottomBarViewBox.add(addBtn, { expand: false, x_fill: false, y_fill: false });

        // 2. Edit Mode: [ Entry ] [Save] [X]
        this.bottomBarEditBox = new St.BoxLayout({
            vertical: false,
            style_class: 'memo-bottom-bar',
            x_expand: true
        });
        this.bottomBarEditBox.hide();

        this.addEntry = new St.Entry({
            hint_text: "New task...",
            can_focus: true,
            style_class: 'memo-add-entry',
            x_expand: true
        });
        // Handle Enter key
        this.addEntry.clutter_text.connect('key-press-event', Lang.bind(this, (actor, event) => {
            let symbol = event.get_key_symbol();
            if (symbol === Clutter.KEY_Return || symbol === Clutter.KEY_KP_Enter) {
                this._saveNewItem();
                return true;
            }
            if (symbol === Clutter.KEY_Escape) {
                this._hideAddEntry();
                return true;
            }
            return false;
        }));

        let saveBtn = new St.Button({
            style_class: 'memo-bottom-button',
            reactive: true,
            can_focus: true
        });
        let saveLabel = new St.Label({ text: "Save" });
        saveBtn.set_child(saveLabel);
        saveBtn.connect('clicked', Lang.bind(this, this._saveNewItem));

        let cancelBtn = new St.Button({
            style_class: 'memo-bottom-button',
            reactive: true,
            can_focus: true
        });
        let cancelLabel = new St.Label({ text: " X " });
        cancelBtn.set_child(cancelLabel);
        cancelBtn.connect('clicked', Lang.bind(this, this._hideAddEntry));

        this.bottomBarEditBox.add(this.addEntry, { expand: true, x_fill: true, y_fill: false });
        this.bottomBarEditBox.add(saveBtn, { expand: false, x_fill: false, y_fill: false });
        this.bottomBarEditBox.add(cancelBtn, { expand: false, x_fill: false, y_fill: false });

        container.add_actor(this.bottomBarViewBox);
        container.add_actor(this.bottomBarEditBox);

        // Standard way to add a full-width actor to a MenuItem logic
        this.bottomBarItem.addActor(container, { expand: true, span: -1 });

        this.menu.addMenuItem(this.bottomBarItem);
    }

    _showAddEntry() {
        this.bottomBarViewBox.hide();
        this.bottomBarEditBox.show();
        this.addEntry.grab_key_focus();
        this.addEntry.set_text("");
    }

    _hideAddEntry() {
        this.bottomBarEditBox.hide();
        this.bottomBarViewBox.show();
    }

    _saveNewItem() {
        let text = this.addEntry.get_text().trim();
        if (!text) {
            this._hideAddEntry();
            return;
        }

        let newLine = `☐ ${text}`;
        let fullLines = [];
        for (let it of this.items) {
            for (let l of it.lines) {
                fullLines.push(l);
            }
        }

        let newContent = "";
        if (fullLines.length === 0) {
            newContent = newLine;
        } else {
            newContent = fullLines.join('\n') + '\n' + newLine;
        }

        // Patch immediately
        this._patchMemo(newContent);
        this._hideAddEntry();

        // Optimistic refresh
        this._buildPopupUI(newContent);
    }

    _bindSettings() {
        this.settings.bind("server-url", "serverUrl", Lang.bind(this, this._onSettingsChanged));
        this.settings.bind("auth-token", "authToken", Lang.bind(this, this._onSettingsChanged));
        this.settings.bind("memo-id", "memoId", Lang.bind(this, this._onSettingsChanged));
        this.settings.bind("refresh-interval", "refreshInterval", Lang.bind(this, this._onSettingsChanged));

        this.settings.bind("popup-font-size", "popupFontSize", Lang.bind(this, this._onStyleSettingsChanged));
        this.settings.bind("popup-width", "popupWidth", Lang.bind(this, this._onStyleSettingsChanged));
        this.settings.bind("scroll-interval", "scrollInterval", Lang.bind(this, this._onScrollSettingsChanged));

        this.settings.bind("set-panel-width", "setPanelWidth", Lang.bind(this, this._onStyleSettingsChanged));
        this.settings.bind("panel-width", "panelWidth", Lang.bind(this, this._onStyleSettingsChanged));
    }

    _onSettingsChanged() {
        this._fetchMemo();
    }

    _onStyleSettingsChanged() {
        this._updateStyles();
    }

    _onScrollSettingsChanged() {
        this._startScrollLoop();
    }

    _updateStyles() {
        let fontSize = this.popupFontSize || 11;
        let width = this.popupWidth || 300;

        let style = `font-size: ${fontSize}pt; min-width: ${width}px; max-width: ${width}px; text-align: left;`;
        this.contentLabel.set_style(style);

        if (this.contentBox) {
            let children = this.contentBox.get_children();
            for (let child of children) {
                // Handle new row containers (BoxLayout)
                if (child instanceof St.BoxLayout) {
                    let rowChildren = child.get_children();
                    for (let rc of rowChildren) {
                        if (rc._isTodoButton && rc.get_child()) {
                            rc.get_child().set_style(`font-size: ${fontSize}pt; text-align: left;`);
                        } else if (rc instanceof St.Label && rc !== this.contentLabel) {
                            rc.set_style(`font-size: ${fontSize}pt; text-align: left;`);
                        }
                    }
                } else if (child._isTodoButton) {
                    if (child.get_child()) {
                        child.get_child().set_style(`font-size: ${fontSize}pt; text-align: left;`);
                    }
                } else if (child instanceof St.Label && child !== this.contentLabel) {
                    child.set_style(`font-size: ${fontSize}pt; text-align: left;`);
                }
            }
        }

        if (this.setPanelWidth) {
            let pWidth = this.panelWidth || 150;
            this.actor.set_style(`min-width: ${pWidth}px; max-width: ${pWidth}px; width: ${pWidth}px; text-align: left;`);

            if (this._applet_label) {
                this._applet_label.clutter_text.ellipsize = Pango.EllipsizeMode.END;
            }
        } else {
            this.actor.set_style(null);
        }
    }

    _updateLoop() {
        this._fetchMemo();
        let interval = this.refreshInterval || 10;
        if (interval < 1) interval = 1;

        if (this._timeoutId) {
            Mainloop.source_remove(this._timeoutId);
            this._timeoutId = null;
        }

        this._timeoutId = Mainloop.timeout_add_seconds(interval * 60, Lang.bind(this, this._updateLoop));
    }

    _startScrollLoop() {
        if (this._scrollTimeoutId) {
            Mainloop.source_remove(this._scrollTimeoutId);
            this._scrollTimeoutId = null;
        }

        let interval = this.scrollInterval || 5;
        if (interval < 1) interval = 1;

        this._scrollTimeoutId = Mainloop.timeout_add_seconds(interval, Lang.bind(this, this._scrollLoop));
    }

    _scrollLoop() {
        if (this.memoLines.length > 0) {
            this.currentLineIndex = (this.currentLineIndex + 1) % this.memoLines.length;
            this._updateAppletLabel();
        }
        return true;
    }

    _updateAppletLabel() {
        if (this.memoLines.length === 0) {
            this.set_applet_label("Empty Memo");
            return;
        }

        let line = this.memoLines[this.currentLineIndex];
        if (line.length > 100) line = line.substring(0, 97) + "...";

        this.set_applet_label(line);
        this.set_applet_tooltip(line);
    }

    _openInBrowser() {
        if (!this.serverUrl || !this.memoId) return;
        let url = this.serverUrl.replace(/\/$/, "");
        let targetUrl = `${url}/m/${this.memoId}`;
        try {
            Gio.app_info_launch_default_for_uri(targetUrl, null);
        } catch (e) {
            global.logError("Error opening browser: " + e);
        }
    }

    _onMenuOpenStateChanged(menu, open) {
        if (!open) {
            this._hideAddEntry();
            if (this._dirty) {
                this._saveChanges();
            }
        }
    }

    _saveChanges() {
        let fullLines = [];
        for (let it of this.items) {
            for (let l of it.lines) {
                fullLines.push(l);
            }
        }

        let newContent = fullLines.join('\n');
        this._patchMemo(newContent);
        this._dirty = false;
    }

    _fetchMemo() {
        if (!this.serverUrl || !this.authToken || !this.memoId) {
            this.set_applet_label("Configure Settings");
            this.contentLabel.set_text("Please configure Server URL, Token and Memo ID in settings.");
            return;
        }

        let url = this.serverUrl.replace(/\/$/, "");
        let apiUrl = `${url}/api/v1/memos/${this.memoId}`;

        let message = Soup.Message.new('GET', apiUrl);
        message.request_headers.append('Authorization', `Bearer ${this.authToken}`);
        message.request_headers.append('Accept', 'application/json');

        this._sendRequest(message, (text) => this._parseResponse(text));
    }

    _patchMemo(newContent) {
        if (!this.serverUrl || !this.authToken || !this.memoId) return;

        let url = this.serverUrl.replace(/\/$/, "");
        let apiUrl = `${url}/api/v1/memos/${this.memoId}`;

        let message = Soup.Message.new('PATCH', apiUrl);
        message.request_headers.append('Authorization', `Bearer ${this.authToken}`);
        message.request_headers.append('Content-Type', 'application/json');

        let body = JSON.stringify({ content: newContent });

        if (Soup.MAJOR_VERSION === 2) {
            message.set_request('application/json', 2, body);
        } else {
            message.set_request_body_from_bytes('application/json', new GLib.Bytes(body));
        }

        this._sendRequest(message, (text) => {
            this._parseResponse(text);
        });
    }

    _sendRequest(message, callback) {
        if (Soup.MAJOR_VERSION === 2) {
            this._httpSession.queue_message(message, (session, msg) => {
                if (msg.status_code !== 200) {
                    this._handleError(`Error ${msg.status_code}`);
                    return;
                }
                if (msg.response_body.data) {
                    callback(msg.response_body.data);
                }
            });
        } else {
            this._httpSession.send_and_read_async(message, GLib.PRIORITY_DEFAULT, null, (session, result) => {
                try {
                    let bytes = session.send_and_read_finish(result);
                    if (message.status_code !== 200) {
                        this._handleError(`Error ${message.status_code}`);
                        return;
                    }
                    let decoder = new TextDecoder('utf-8');
                    let text = decoder.decode(bytes.get_data());
                    callback(text);
                } catch (e) {
                    this._handleError("Connection Error");
                }
            });
        }
    }

    _handleError(msg) {
        this.set_applet_label(msg);
        this.contentLabel.set_text(msg);
        this.memoLines = [];
    }

    _parseResponse(jsonString) {
        try {
            let data = JSON.parse(jsonString);

            let content = data.content || (data.memo && data.memo.content) || "";

            if (this._dirty) return;

            this.memoLines = content.split('\n').map(l => l.trim()).filter(l => l.length > 0);

            this._buildPopupUI(content);

            if (this.currentLineIndex >= this.memoLines.length) {
                this.currentLineIndex = 0;
            }
            this._updateAppletLabel();

        } catch (e) {
            this._handleError("Parse Error");
        }
    }

    _buildPopupUI(content) {
        this.contentBox.destroy_all_children();
        this.items = [];

        let lines = content.split('\n');
        let currentItem = null;

        for (let line of lines) {
            let isTodoStart = line.startsWith("☐ ") || line.startsWith("☑ ");

            if (isTodoStart) {
                currentItem = {
                    type: 'todo',
                    checked: line.startsWith("☑ "),
                    lines: [line]
                };
                this.items.push(currentItem);
            } else {
                if (currentItem) {
                    currentItem.lines.push(line);
                } else {
                    currentItem = { type: 'text', lines: [line] };
                    this.items.push(currentItem);
                }
            }
        }

        for (let i = 0; i < this.items.length; i++) {
            let item = this.items[i];
            let text = item.lines.join('\n');

            if (item.type === 'text') {
                let label = new St.Label({ text: text, style_class: 'memo-popup-text' });
                label.set_style("text-align: left;");
                label.clutter_text.line_wrap = true;
                label.clutter_text.line_wrap_mode = Pango.WrapMode.WORD_CHAR;
                this.contentBox.add(label);
            } else if (item.type === 'todo') {
                let itemBox = new St.BoxLayout({ vertical: false, x_expand: true });

                let btn = new St.Button({
                    style_class: 'memo-todo-button',
                    reactive: true,
                    can_focus: true,
                    x_expand: true
                });
                btn._isTodoButton = true;

                let btnLabel = new St.Label({ text: text, style_class: 'memo-popup-text' });
                btnLabel.set_style("text-align: left;");
                btnLabel.clutter_text.line_wrap = true;
                btnLabel.clutter_text.line_wrap_mode = Pango.WrapMode.WORD_CHAR;

                btn.set_child(btnLabel);
                item.labelActor = btnLabel;

                let index = i;
                btn.connect('clicked', Lang.bind(this, () => this._onTodoClicked(index)));

                let deleteBtn = new St.Button({
                    style_class: 'memo-delete-button',
                    reactive: true,
                    can_focus: true,
                    x_expand: false
                });
                deleteBtn.set_child(new St.Label({ text: " 🗑 " }));
                deleteBtn.connect('clicked', Lang.bind(this, () => this._onDeleteClicked(index)));

                itemBox.add(btn, { expand: true, x_fill: true, y_fill: false });
                itemBox.add(deleteBtn, { expand: false, x_fill: false, y_fill: false });

                this.contentBox.add(itemBox);
            }
        }

        this._updateStyles();
    }

    _onTodoClicked(index) {
        let item = this.items[index];
        if (!item || item.type !== 'todo') return;

        item.checked = !item.checked;
        item.lines[0] = (item.checked ? "☑ " : "☐ ") + item.lines[0].substring(2);

        if (item.labelActor) {
            item.labelActor.set_text(item.lines.join('\n'));
        }

        this._dirty = true;
    }

    _onDeleteClicked(index) {
        this.items.splice(index, 1);
        this._dirty = true;

        // Rebuild content string from remaining items
        let newContent = this.items.map(it => it.lines.join('\n')).join('\n');
        this._buildPopupUI(newContent);
    }

    on_applet_clicked(event) {
        this.menu.toggle();
    }

    on_applet_removed_from_panel() {
        if (this._timeoutId) {
            Mainloop.source_remove(this._timeoutId);
            this._timeoutId = null;
        }
        if (this._scrollTimeoutId) {
            Mainloop.source_remove(this._scrollTimeoutId);
            this._scrollTimeoutId = null;
        }
        this.settings.finalize();
    }
}

function main(metadata, orientation, panel_height, instance_id) {
    return new MemosApplet(metadata, orientation, panel_height, instance_id);
}
