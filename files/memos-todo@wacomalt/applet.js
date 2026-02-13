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
const Gettext = imports.gettext;

const UUID = "memos-todo@wacomalt";

function _(str) {
    return Gettext.dgettext(UUID, str);
}

class MemosApplet extends Applet.TextApplet {
    constructor(metadata, orientation, panel_height, instance_id) {
        super(orientation, panel_height, instance_id);

        this.set_applet_label(_("Loading..."));
        this.actor.set_clip_to_allocation(true);

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

        // Use a persistent status label that we don't destroy
        this.statusLabel = new St.Label({
            text: _("Loading..."),
            style_class: 'memo-popup-text',
            x_align: St.Align.START
        });
        this.contentBox.add(this.statusLabel);

        // Sub-container for actual items (this one we CAN destroy children of)
        this.itemsBox = new St.BoxLayout({ vertical: true, x_expand: true });
        this.contentBox.add(this.itemsBox);

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
        this.bottomBarItem.activate = () => false;

        let container = new St.BoxLayout({ vertical: true, x_expand: true, style_class: 'memo-bottom-bar' });

        // Row 1: [ Entry ] [+]
        let entryBin = new St.BoxLayout({ vertical: false, x_expand: true });

        this.addEntry = new St.Entry({
            hint_text: _("New task..."),
            can_focus: true,
            style_class: 'memo-add-entry',
            x_expand: true
        });

        this.addEntry.clutter_text.connect('key-press-event', (actor, event) => {
            let symbol = event.get_key_symbol();
            if (symbol === Clutter.KEY_Return || symbol === Clutter.KEY_KP_Enter) {
                this._saveNewItem();
                return true;
            }
            return false;
        });

        let addBtn = new St.Button({
            style_class: 'memo-bottom-button',
            reactive: true,
            can_focus: true,
            x_expand: false
        });
        this.addLabel = new St.Label({ text: " + " });
        addBtn.set_child(this.addLabel);
        addBtn.connect('clicked', () => this._saveNewItem());

        entryBin.add(this.addEntry, { expand: true, x_fill: true, y_fill: false });
        entryBin.add(addBtn, { expand: false, x_fill: false, y_fill: false });

        // Row 2: [Open in Browser]
        let browserBtn = new St.Button({
            style_class: 'memo-bottom-button',
            reactive: true,
            can_focus: true,
            x_expand: true,
            x_align: St.Align.START
        });
        this.browserLabel = new St.Label({ text: _("Open in Browser"), x_align: St.Align.START });
        browserBtn.set_child(this.browserLabel);
        browserBtn.connect('clicked', () => this._openInBrowser());

        container.add_actor(entryBin);
        container.add_actor(browserBtn);

        this.bottomBarItem.addActor(container, { expand: true, span: -1 });
        this.menu.addMenuItem(this.bottomBarItem);
    }


    _saveNewItem() {
        let text = this.addEntry.get_text().trim();
        if (!text) return;

        let newLine = `☐ ${text}`;
        let fullLines = [];
        for (let it of this.items) {
            for (let l of it.lines) {
                fullLines.push(l);
            }
        }

        let newContent = (fullLines.length === 0) ? newLine : fullLines.join('\n') + '\n' + newLine;
        this._patchMemo(newContent);
        this.addEntry.set_text("");

        // Immediate panel/UI sync
        this._parseResponse(JSON.stringify({ content: newContent }), true);
    }

    _bindSettings() {
        this.settings.bind("server-url", "serverUrl", Lang.bind(this, this._onSettingsChanged));
        this.settings.bind("auth-token", "authToken", Lang.bind(this, this._onSettingsChanged));
        this.settings.bind("memo-id", "memoId", Lang.bind(this, this._onSettingsChanged));
        this.settings.bind("refresh-interval", "refreshInterval", Lang.bind(this, this._onSettingsChanged));
        this.settings.bind("panel-font-size", "panelFontSize", Lang.bind(this, this._onStyleSettingsChanged));
        this.settings.bind("popup-font-size", "popupFontSize", Lang.bind(this, this._onStyleSettingsChanged));
        this.settings.bind("popup-width", "popupWidth", Lang.bind(this, this._onStyleSettingsChanged));
        this.settings.bind("scroll-interval", "scrollInterval", Lang.bind(this, this._onScrollSettingsChanged));
        this.settings.bind("set-panel-width", "setPanelWidth", Lang.bind(this, this._onStyleSettingsChanged));
        this.settings.bind("panel-width", "panelWidth", Lang.bind(this, this._onStyleSettingsChanged));
        this.settings.bind("transition-duration", "transitionDuration", Lang.bind(this, this._onStyleSettingsChanged));
        this.settings.bind("show-completed-panel", "showCompletedPanel", Lang.bind(this, this._onSettingsChanged));
        this.settings.bind("show-completed-popup", "showCompletedPopup", Lang.bind(this, this._onSettingsChanged));
    }

    _onSettingsChanged() { this._fetchMemo(); }
    _onStyleSettingsChanged() { this._updateStyles(); }
    _onScrollSettingsChanged() { this._startScrollLoop(); }

    _updateStyles() {
        let fontSize = this.popupFontSize || 11;
        let panelFontSize = this.panelFontSize || 10;
        let width = this.popupWidth || 300;

        let style = `font-size: ${fontSize}pt; min-width: ${width}px; max-width: ${width}px; text-align: left;`;
        if (this.statusLabel && !this.statusLabel.is_finalizing) {
            this.statusLabel.set_style(style);
        }

        // Apply to Bottom Bar
        if (this.browserLabel) this.browserLabel.set_style(`font-size: ${fontSize}pt; text-align: left;`);
        if (this.addLabel) this.addLabel.set_style(`font-size: ${fontSize}pt; text-align: left;`);
        if (this.addEntry) this.addEntry.set_style(`font-size: ${fontSize}pt;`);

        if (this.itemsBox) {
            let children = this.itemsBox.get_children();
            for (let child of children) {
                if (child instanceof St.BoxLayout) {
                    let rowChildren = child.get_children();
                    for (let rc of rowChildren) {
                        this._applyStyleToChild(rc, fontSize);
                    }
                } else {
                    this._applyStyleToChild(child, fontSize);
                }
            }
        }

        let panelStyle = `font-size: ${panelFontSize}pt; text-align: left;`;
        if (this.setPanelWidth) {
            let pWidth = this.panelWidth || 150;
            panelStyle += ` min-width: ${pWidth}px; max-width: ${pWidth}px; width: ${pWidth}px;`;

            if (this._applet_label) this._applet_label.clutter_text.ellipsize = Pango.EllipsizeMode.END;
        }
        this.actor.set_style(panelStyle);
    }

    _applyStyleToChild(rc, fontSize) {
        if (!rc || rc.is_finalizing) return;
        let style = `font-size: ${fontSize}pt; text-align: left;`;

        if (rc._isTodoButton) {
            let child = rc.get_child();
            if (child) {
                child.set_style(style);
                if ('x_align' in child) child.x_align = St.Align.START;
            }
            if ('x_align' in rc) rc.x_align = St.Align.START;
        } else if (rc instanceof St.Label) {
            rc.set_style(style);
            if ('x_align' in rc) rc.x_align = St.Align.START;
        }
    }

    _updateLoop() {
        this._fetchMemo();
        let interval = this.refreshInterval || 10;
        if (interval < 1) interval = 1;
        if (this._timeoutId) Mainloop.source_remove(this._timeoutId);
        this._timeoutId = Mainloop.timeout_add_seconds(interval * 60, Lang.bind(this, this._updateLoop));
    }

    _startScrollLoop() {
        if (this._scrollTimeoutId) Mainloop.source_remove(this._scrollTimeoutId);
        let interval = this.scrollInterval || 5;
        this._scrollTimeoutId = Mainloop.timeout_add_seconds(interval, Lang.bind(this, this._scrollLoop));
    }

    _scrollLoop() {
        if (this.memoLines.length <= 1) {
            if (this.memoLines.length === 1) this._updateAppletLabel();
            return true;
        }

        let duration = this.transitionDuration;
        if (duration === undefined) duration = 300;

        if (duration > 0 && this._applet_label) {
            this._applet_label.ease({
                opacity: 0,
                translation_y: -20,
                duration: duration,
                mode: Clutter.AnimationMode.EASE_OUT_QUAD,
                onComplete: () => {
                    if (this.memoLines.length > 0) {
                        this.currentLineIndex = (this.currentLineIndex + 1) % this.memoLines.length;
                    } else {
                        this.currentLineIndex = 0;
                    }
                    if (isNaN(this.currentLineIndex)) this.currentLineIndex = 0;

                    this._updateAppletLabel();
                    if (!this._applet_label) return;

                    this._applet_label.set_translation(0, 20, 0);
                    this._applet_label.ease({
                        opacity: 255,
                        translation_y: 0,
                        duration: duration,
                        mode: Clutter.AnimationMode.EASE_IN_QUAD
                    });
                }
            });
        } else {
            this.currentLineIndex = (this.currentLineIndex + 1) % this.memoLines.length;
            this._updateAppletLabel();
        }
        return true;
    }

    _updateAppletLabel() {
        if (!this.memoLines || this.memoLines.length === 0) {
            this.set_applet_label(_("Empty Memo"));
            return;
        }

        // Ensure index is valid and not NaN
        if (isNaN(this.currentLineIndex) || this.currentLineIndex < 0 || this.currentLineIndex >= this.memoLines.length) {
            this.currentLineIndex = 0;
        }

        let line = this.memoLines[this.currentLineIndex];
        if (!line) {
            this.set_applet_label(_("Empty Memo"));
            return;
        }

        if (line.length > 100) line = line.substring(0, 97) + "...";
        this.set_applet_label(line);
    }

    _openInBrowser() {
        if (!this.serverUrl || !this.memoId) return;
        let url = this.serverUrl.replace(/\/$/, "");
        let targetUrl = `${url}/m/${this.memoId}`;
        try { Gio.app_info_launch_default_for_uri(targetUrl, null); } catch (e) { global.logError(e); }
    }


    _saveChanges() {
        let fullLines = [];
        for (let it of this.items) for (let l of it.lines) fullLines.push(l);
        this._patchMemo(fullLines.join('\n'));
        this._dirty = false;
    }

    _fetchMemo() {
        if (!this.serverUrl || !this.authToken || !this.memoId) {
            this.set_applet_label("Configure Settings");
            this.statusLabel.set_text("Please configure Server URL, Token and Memo ID in settings.");
            this.statusLabel.show();
            return;
        }
        let apiUrl = `${this.serverUrl.replace(/\/$/, "")}/api/v1/memos/${this.memoId}`;
        let message = Soup.Message.new('GET', apiUrl);
        message.request_headers.append('Authorization', `Bearer ${this.authToken}`);
        message.request_headers.append('Accept', 'application/json');
        this._sendRequest(message, (text) => this._parseResponse(text));
    }

    _patchMemo(newContent) {
        if (!this.serverUrl || !this.authToken || !this.memoId) return;
        let apiUrl = `${this.serverUrl.replace(/\/$/, "")}/api/v1/memos/${this.memoId}`;
        let message = Soup.Message.new('PATCH', apiUrl);
        message.request_headers.append('Authorization', `Bearer ${this.authToken}`);
        message.request_headers.append('Content-Type', 'application/json');
        let body = JSON.stringify({ content: newContent });

        if (Soup.MAJOR_VERSION === 2) {
            message.set_request('application/json', 1, body); // Use COPY (1) instead of TAKE (2)
        } else {
            message.set_request_body_from_bytes('application/json', new GLib.Bytes(body));
        }

        this._sendRequest(message, (text) => {
            log("Memos ToDo: PATCH Successful");
            this._parseResponse(text, true);
        });
    }

    _sendRequest(message, callback) {
        if (Soup.MAJOR_VERSION === 2) {
            this._httpSession.queue_message(message, (session, msg) => {
                if (msg.status_code !== 200) { this._handleError(`Error ${msg.status_code}`); return; }
                if (msg.response_body.data) callback(msg.response_body.data);
            });
        } else {
            this._httpSession.send_and_read_async(message, GLib.PRIORITY_DEFAULT, null, (session, result) => {
                try {
                    let bytes = session.send_and_read_finish(result);
                    if (message.status_code !== 200) {
                        this._handleError(_("Error %d").format(message.status_code));
                        return;
                    }
                    callback(new TextDecoder('utf-8').decode(bytes.get_data()));
                } catch (e) { this._handleError(_("Connection Error")); }
            });
        }
    }

    _handleError(msg) {
        this.set_applet_label(msg);
        this.statusLabel.set_text(msg);
        this.statusLabel.show();
        this.memoLines = [];
    }

    _parseResponse(jsonString, force = false) {
        let data;
        try {
            data = JSON.parse(jsonString);
            if (!data) throw new Error("Null or empty JSON");

            let content = data.content || (data.memo && data.memo.content) || "";

            if (this._dirty && !force) return;

            let allLines = content.split('\n').map(l => l.trim()).filter(l => l.length > 0);

            // Filter panel lines
            if (!this.showCompletedPanel) {
                this.memoLines = allLines.filter(l => !l.startsWith("☑ "));
            } else {
                this.memoLines = allLines;
            }

            this._buildPopupUI(content);
            if (this.currentLineIndex >= this.memoLines.length) this.currentLineIndex = 0;
            this._updateAppletLabel();
        } catch (e) {
            log("Memos ToDo Parse Error: " + e.message + " | Data: " + jsonString.substring(0, 100));
            this._handleError("Parse Error");
        }
    }

    _buildPopupUI(content) {
        this.itemsBox.destroy_all_children();
        this.statusLabel.hide();
        this.items = [];
        let lines = content.split('\n');
        let currentItem = null;
        for (let line of lines) {
            let lineTrim = line.trim();
            if (lineTrim === "") continue;

            let isTodoStart = lineTrim.startsWith("☐ ") || lineTrim.startsWith("☑ ");
            if (isTodoStart) {
                let checked = lineTrim.startsWith("☑ ");
                if (!this.showCompletedPopup && checked) {
                    currentItem = null; // Don't append content to a hidden todo
                    continue;
                }
                currentItem = { type: 'todo', checked: checked, lines: [lineTrim] };
                this.items.push(currentItem);
            } else {
                if (currentItem) currentItem.lines.push(lineTrim);
                else { currentItem = { type: 'text', lines: [lineTrim] }; this.items.push(currentItem); }
            }
        }

        this.items.forEach((item, i) => {
            let text = item.lines.join('\n');
            if (item.type === 'text') {
                let label = new St.Label({ text: text, style_class: 'memo-popup-text', x_align: St.Align.START });
                label.clutter_text.line_wrap = true;
                label.clutter_text.line_wrap_mode = Pango.WrapMode.WORD_CHAR;
                this.itemsBox.add(label);
            } else {
                let itemBox = new St.BoxLayout({ vertical: false, x_expand: true });
                let btn = new St.Button({ style_class: 'memo-todo-button', reactive: true, can_focus: true, x_expand: true, x_align: St.Align.START });
                btn._isTodoButton = true;
                let btnLabel = new St.Label({
                    text: text,
                    style_class: 'memo-popup-text' + (item.checked ? ' memo-todo-completed' : ''),
                    x_align: St.Align.START
                });
                btnLabel.clutter_text.line_wrap = true;
                btnLabel.clutter_text.line_wrap_mode = Pango.WrapMode.WORD_CHAR;
                btn.set_child(btnLabel);
                item.labelActor = btnLabel;
                btn.connect('clicked', Lang.bind(this, () => this._onTodoClicked(i)));
                let deleteBtn = new St.Button({ style_class: 'memo-delete-button', reactive: true, can_focus: true, x_expand: false });
                deleteBtn.set_child(new St.Label({ text: " 🗑 " }));
                deleteBtn.connect('clicked', Lang.bind(this, () => this._onDeleteClicked(i)));
                itemBox.add(btn, { expand: true, x_fill: true, y_fill: false });
                itemBox.add(deleteBtn, { expand: false, x_fill: false, y_fill: false });
                this.itemsBox.add(itemBox);
            }
        });
        this._updateStyles();
    }

    _onTodoClicked(index) {
        let item = this.items[index];
        if (!item || item.type !== 'todo') return;
        item.checked = !item.checked;
        item.lines[0] = (item.checked ? "☑ " : "☐ ") + item.lines[0].substring(2);
        if (item.labelActor) {
            item.labelActor.set_text(item.lines.join('\n'));
            if (item.checked) item.labelActor.add_style_class_name('memo-todo-completed');
            else item.labelActor.remove_style_class_name('memo-todo-completed');
        }
        this._saveChanges();
    }

    _onDeleteClicked(index) {
        this.items.splice(index, 1);
        this._saveChanges();
    }

    on_applet_clicked(event) { this.menu.toggle(); }
    on_applet_removed_from_panel() {
        if (this._timeoutId) Mainloop.source_remove(this._timeoutId);
        if (this._scrollTimeoutId) Mainloop.source_remove(this._scrollTimeoutId);
        this.settings.finalize();
    }
}

function main(metadata, orientation, panel_height, instance_id) {
    return new MemosApplet(metadata, orientation, panel_height, instance_id);
}
