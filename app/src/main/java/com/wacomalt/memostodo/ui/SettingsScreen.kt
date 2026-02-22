package com.wacomalt.memostodo.ui

import androidx.compose.foundation.layout.*
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.ArrowBack
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import com.wacomalt.memostodo.SettingsManager
import kotlinx.coroutines.launch

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun SettingsScreen(settingsManager: SettingsManager, onBack: () -> Unit) {
    val scope = rememberCoroutineScope()
    
    val savedUrl by settingsManager.serverUrl.collectAsState(initial = "")
    val savedToken by settingsManager.authToken.collectAsState(initial = "")
    val savedId by settingsManager.memoId.collectAsState(initial = "")
    val savedRefresh by settingsManager.refreshInterval.collectAsState(initial = 10)
    val savedAppFont by settingsManager.appFontSize.collectAsState(initial = 11)
    val savedWidgetFont by settingsManager.widgetFontSize.collectAsState(initial = 10)
    val savedScroll by settingsManager.scrollInterval.collectAsState(initial = 5)
    val savedShowApp by settingsManager.showCompletedApp.collectAsState(initial = true)
    val savedShowWidget by settingsManager.showCompletedWidget.collectAsState(initial = true)

    var url by remember { mutableStateOf("") }
    var token by remember { mutableStateOf("") }
    var memoId by remember { mutableStateOf("") }
    var refresh by remember { mutableStateOf("10") }
    var appFont by remember { mutableStateOf("11") }
    var widgetFont by remember { mutableStateOf("10") }
    var scroll by remember { mutableStateOf("5") }
    var showApp by remember { mutableStateOf(true) }
    var showWidget by remember { mutableStateOf(true) }

    LaunchedEffect(savedUrl, savedToken, savedId, savedRefresh, savedAppFont, savedWidgetFont, savedScroll, savedShowApp, savedShowWidget) {
        url = savedUrl
        token = savedToken
        memoId = savedId
        refresh = savedRefresh.toString()
        appFont = savedAppFont.toString()
        widgetFont = savedWidgetFont.toString()
        scroll = savedScroll.toString()
        showApp = savedShowApp
        showWidget = savedShowWidget
    }

    Scaffold(
        topBar = {
            TopAppBar(
                title = { Text("Configuration") },
                navigationIcon = {
                    IconButton(onClick = onBack) {
                        Icon(Icons.Default.ArrowBack, contentDescription = "Back")
                    }
                }
            )
        }
    ) { padding ->
        Column(
            modifier = Modifier
                .padding(padding)
                .padding(16.dp)
                .fillMaxSize()
                .verticalScroll(rememberScrollState()),
            verticalArrangement = Arrangement.spacedBy(16.dp)
        ) {
            Text("Connection", style = MaterialTheme.typography.titleMedium)
            TextField(value = url, onValueChange = { url = it }, label = { Text("Server URL") }, modifier = Modifier.fillMaxWidth())
            TextField(value = token, onValueChange = { token = it }, label = { Text("Auth Token") }, modifier = Modifier.fillMaxWidth())
            TextField(value = memoId, onValueChange = { memoId = it }, label = { Text("Memo ID") }, modifier = Modifier.fillMaxWidth())
            
            Divider()
            Text("Refresh & Intervals", style = MaterialTheme.typography.titleMedium)
            TextField(value = refresh, onValueChange = { refresh = it }, label = { Text("Refresh Interval (min)") }, modifier = Modifier.fillMaxWidth())
            TextField(value = scroll, onValueChange = { scroll = it }, label = { Text("Widget Scroll Interval (sec)") }, modifier = Modifier.fillMaxWidth())

            Divider()
            Text("Appearance", style = MaterialTheme.typography.titleMedium)
            TextField(value = appFont, onValueChange = { appFont = it }, label = { Text("App Font Size (sp)") }, modifier = Modifier.fillMaxWidth())
            TextField(value = widgetFont, onValueChange = { widgetFont = it }, label = { Text("Widget Font Size (sp)") }, modifier = Modifier.fillMaxWidth())

            Divider()
            Text("Visibility", style = MaterialTheme.typography.titleMedium)
            Row(verticalAlignment = Alignment.CenterVertically) {
                Checkbox(checked = showApp, onCheckedChange = { showApp = it })
                Text("Show Completed in App")
            }
            Row(verticalAlignment = Alignment.CenterVertically) {
                Checkbox(checked = showWidget, onCheckedChange = { showWidget = it })
                Text("Show Completed in Widgets")
            }

            Button(
                onClick = {
                    scope.launch {
                        settingsManager.saveSettings(
                            url, token, memoId,
                            refresh.toIntOrNull() ?: 10,
                            appFont.toIntOrNull() ?: 11,
                            widgetFont.toIntOrNull() ?: 10,
                            scroll.toIntOrNull() ?: 5,
                            showApp, showWidget
                        )
                        onBack()
                    }
                },
                modifier = Modifier.fillMaxWidth().padding(top = 16.dp)
            ) {
                Text("Save Settings")
            }
        }
    }
}
