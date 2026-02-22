package com.wacomalt.memostodo

import android.content.Context
import androidx.datastore.core.DataStore
import androidx.datastore.preferences.core.Preferences
import androidx.datastore.preferences.core.edit
import androidx.datastore.preferences.core.stringPreferencesKey
import androidx.datastore.preferences.core.intPreferencesKey
import androidx.datastore.preferences.core.booleanPreferencesKey
import androidx.datastore.preferences.preferencesDataStore
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.map

val Context.dataStore: DataStore<Preferences> by preferencesDataStore(name = "settings")

class SettingsManager(private val context: Context) {
    companion object {
        val SERVER_URL = stringPreferencesKey("server_url")
        val AUTH_TOKEN = stringPreferencesKey("auth_token")
        val MEMO_ID = stringPreferencesKey("memo_id")
        val REFRESH_INTERVAL = intPreferencesKey("refresh_interval")
        val APP_FONT_SIZE = intPreferencesKey("app_font_size")
        val WIDGET_FONT_SIZE = intPreferencesKey("widget_font_size")
        val SCROLL_INTERVAL = intPreferencesKey("scroll_interval")
        val SHOW_COMPLETED_APP = booleanPreferencesKey("show_completed_app")
        val SHOW_COMPLETED_WIDGET = booleanPreferencesKey("show_completed_widget")
    }

    val serverUrl: Flow<String> = context.dataStore.data.map { it[SERVER_URL] ?: "https://demo.usememos.com" }
    val authToken: Flow<String> = context.dataStore.data.map { it[AUTH_TOKEN] ?: "" }
    val memoId: Flow<String> = context.dataStore.data.map { it[MEMO_ID] ?: "1" }
    val refreshInterval: Flow<Int> = context.dataStore.data.map { it[REFRESH_INTERVAL] ?: 10 }
    val appFontSize: Flow<Int> = context.dataStore.data.map { it[APP_FONT_SIZE] ?: 11 }
    val widgetFontSize: Flow<Int> = context.dataStore.data.map { it[WIDGET_FONT_SIZE] ?: 10 }
    val scrollInterval: Flow<Int> = context.dataStore.data.map { it[SCROLL_INTERVAL] ?: 5 }
    val showCompletedApp: Flow<Boolean> = context.dataStore.data.map { it[SHOW_COMPLETED_APP] ?: true }
    val showCompletedWidget: Flow<Boolean> = context.dataStore.data.map { it[SHOW_COMPLETED_WIDGET] ?: true }

    suspend fun saveSettings(
        url: String, 
        token: String, 
        id: String,
        refresh: Int,
        appFont: Int,
        widgetFont: Int,
        scroll: Int,
        showApp: Boolean,
        showWidget: Boolean
    ) {
        context.dataStore.edit { settings ->
            settings[SERVER_URL] = url
            settings[AUTH_TOKEN] = token
            settings[MEMO_ID] = id
            settings[REFRESH_INTERVAL] = refresh
            settings[APP_FONT_SIZE] = appFont
            settings[WIDGET_FONT_SIZE] = widgetFont
            settings[SCROLL_INTERVAL] = scroll
            settings[SHOW_COMPLETED_APP] = showApp
            settings[SHOW_COMPLETED_WIDGET] = showWidget
        }
    }
}
