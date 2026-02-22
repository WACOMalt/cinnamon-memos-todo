package com.wacomalt.memostodo

import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.itemsIndexed
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Add
import androidx.compose.material.icons.filled.Delete
import androidx.compose.material.icons.filled.Refresh
import androidx.compose.material.icons.filled.Settings
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.wacomalt.memostodo.api.MemosRepository
import com.wacomalt.memostodo.model.MemoItem
import com.wacomalt.memostodo.widget.MemoListWidget
import com.wacomalt.memostodo.widget.TaskBarWidget
import androidx.glance.appwidget.updateAll
import kotlinx.coroutines.launch

class MainActivity : ComponentActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        
        val settingsManager = SettingsManager(applicationContext)

        setContent {
            var showSettings by remember { mutableStateOf(false) }
            val serverUrl by settingsManager.serverUrl.collectAsState(initial = "https://demo.usememos.com")
            val authToken by settingsManager.authToken.collectAsState(initial = "")
            val memoId by settingsManager.memoId.collectAsState(initial = "1")
            val appFontSize by settingsManager.appFontSize.collectAsState(initial = 11)
            val showCompletedApp by settingsManager.showCompletedApp.collectAsState(initial = true)

            MaterialTheme(colorScheme = darkColorScheme()) {
                Surface(modifier = Modifier.fillMaxSize(), color = MaterialTheme.colorScheme.background) {
                    if (showSettings) {
                        com.wacomalt.memostodo.ui.SettingsScreen(settingsManager) {
                            showSettings = false
                        }
                    } else {
                        val repo = remember(serverUrl, authToken) { 
                            com.wacomalt.memostodo.api.MemosRepository(serverUrl, authToken) 
                        }
                        MemosScreen(repo, memoId, appFontSize, showCompletedApp, onOpenSettings = { showSettings = true })
                    }
                }
            }
        }
    }
}

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun MemosScreen(
    repository: com.wacomalt.memostodo.api.MemosRepository, 
    memoId: String, 
    fontSize: Int,
    showCompleted: Boolean,
    onOpenSettings: () -> Unit
) {
    val scope = rememberCoroutineScope()
    var rawItems by remember { mutableStateOf(listOf<MemoItem>()) }
    var newTaskText by remember { mutableStateOf("") }
    var isLoading by remember { mutableStateOf(false) }

    val items = remember(rawItems, showCompleted) {
        if (showCompleted) rawItems else rawItems.filter { it is MemoItem.Text || (it is MemoItem.Todo && !it.isChecked) }
    }

    val lifecycleOwner = androidx.compose.ui.platform.LocalLifecycleOwner.current
    val currentRepo by rememberUpdatedState(repository)
    val currentMemoId by rememberUpdatedState(memoId)
    val context = androidx.compose.ui.platform.LocalContext.current

    androidx.compose.runtime.DisposableEffect(lifecycleOwner) {
        val observer = androidx.lifecycle.LifecycleEventObserver { _, event ->
            if (event == androidx.lifecycle.Lifecycle.Event.ON_RESUME) {
                 scope.launch {
                     val result = currentRepo.fetchMemo(currentMemoId)
                     if (result != null) {
                         rawItems = result
                     }
                 }
            } else if (event == androidx.lifecycle.Lifecycle.Event.ON_PAUSE) {
                 scope.launch {
                     MemoListWidget().updateAll(context)
                     TaskBarWidget().updateAll(context)
                 }
            }
        }
        lifecycleOwner.lifecycle.addObserver(observer)
        onDispose {
            lifecycleOwner.lifecycle.removeObserver(observer)
        }
    }

    LaunchedEffect(repository, memoId) {
        if (memoId != "1") {
             isLoading = true
             val result = repository.fetchMemo(memoId)
             if (result != null) {
                 rawItems = result
             }
             isLoading = false
        }
    }

    Scaffold(
        topBar = {
            TopAppBar(
                title = { Text("Memos ToDo") },
                actions = {
                    IconButton(onClick = {
                        scope.launch {
                            isLoading = true
                            val result = repository.fetchMemo(memoId)
                            if (result != null) {
                                rawItems = result
                            }
                            isLoading = false
                        }
                    }) {
                        Icon(Icons.Default.Refresh, contentDescription = "Refresh")
                    }
                    IconButton(onClick = onOpenSettings) {
                        Icon(Icons.Default.Settings, contentDescription = "Settings")
                    }
                }
            )
        },
        bottomBar = {
            BottomAppBar {
                Row(
                    modifier = Modifier.padding(8.dp).fillMaxWidth(),
                    verticalAlignment = Alignment.CenterVertically
                ) {
                    TextField(
                        value = newTaskText,
                        onValueChange = { newTaskText = it },
                        modifier = Modifier.weight(1f),
                        placeholder = { Text("New task...") }
                    )
                    IconButton(onClick = {
                        if (newTaskText.isNotBlank()) {
                            val newList = rawItems + MemoItem.Todo(newTaskText, false)
                            newTaskText = ""
                            scope.launch {
                                repository.updateMemo(memoId, newList)
                                val result = repository.fetchMemo(memoId)
                                if (result != null) {
                                    rawItems = result
                                }
                                MemoListWidget().updateAll(context)
                                TaskBarWidget().updateAll(context)
                            }
                        }
                    }) {
                        Icon(Icons.Default.Add, contentDescription = "Add")
                    }
                }
            }
        }
    ) { padding ->
        if (isLoading) {
            Box(modifier = Modifier.fillMaxSize(), contentAlignment = Alignment.Center) {
                CircularProgressIndicator()
            }
        } else {
            LazyColumn(modifier = Modifier.padding(padding)) {
                itemsIndexed(items) { _, item ->
                    when (item) {
                        is MemoItem.Todo -> TodoRow(item, fontSize,
                            onCheckedChange = { checked ->
                                val newList = rawItems.toMutableList()
                                val rawIndex = rawItems.indexOf(item)
                                if (rawIndex != -1) {
                                    newList[rawIndex] = item.copy(isChecked = checked)
                                    rawItems = newList // Optimistic update
                                    scope.launch {
                                        repository.updateMemo(memoId, newList)
                                        MemoListWidget().updateAll(context)
                                        TaskBarWidget().updateAll(context)
                                    }
                                }
                            },
                            onDelete = {
                                val newList = rawItems.toMutableList()
                                val rawIndex = rawItems.indexOf(item)
                                if (rawIndex != -1) {
                                    newList.removeAt(rawIndex)
                                    rawItems = newList // Optimistic update
                                    scope.launch {
                                        repository.updateMemo(memoId, newList)
                                        MemoListWidget().updateAll(context)
                                        TaskBarWidget().updateAll(context)
                                    }
                                }
                            }
                        )
                        is MemoItem.Text -> TextRow(item, fontSize)
                    }
                }
            }
        }
    }
}

@Composable
fun TodoRow(todo: MemoItem.Todo, fontSize: Int, onCheckedChange: (Boolean) -> Unit, onDelete: () -> Unit) {
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .padding(8.dp),
        verticalAlignment = Alignment.CenterVertically
    ) {
        Checkbox(checked = todo.isChecked, onCheckedChange = onCheckedChange)
        Text(
            text = todo.text,
            modifier = Modifier.weight(1f),
            style = MaterialTheme.typography.bodyLarge.copy(fontSize = fontSize.sp)
        )
        IconButton(onClick = onDelete) {
            Icon(Icons.Default.Delete, contentDescription = "Delete")
        }
    }
}

@Composable
fun TextRow(textItem: MemoItem.Text, fontSize: Int) {
    Text(
        text = textItem.text,
        modifier = Modifier.padding(16.dp).fillMaxWidth(),
        style = MaterialTheme.typography.bodyMedium.copy(fontSize = fontSize.sp)
    )
}
