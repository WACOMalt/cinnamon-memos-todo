package com.wacomalt.memostodo.widget

import android.content.Context
import androidx.glance.GlanceId
import androidx.glance.GlanceModifier
import androidx.glance.appwidget.GlanceAppWidget
import androidx.glance.appwidget.GlanceAppWidgetReceiver
import androidx.glance.appwidget.lazy.LazyColumn
import androidx.glance.appwidget.lazy.items
import androidx.glance.appwidget.provideContent
import androidx.glance.background
import androidx.glance.layout.*
import androidx.glance.text.Text
import androidx.glance.Button
import androidx.glance.action.actionStartActivity
import androidx.glance.appwidget.CheckBox
import androidx.glance.text.TextStyle
import androidx.glance.text.FontWeight
import androidx.glance.text.TextDecoration
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.compose.ui.graphics.Color
import androidx.glance.unit.ColorProvider
import com.wacomalt.memostodo.MainActivity
import com.wacomalt.memostodo.SettingsManager
import com.wacomalt.memostodo.api.MemosRepository
import com.wacomalt.memostodo.model.MemoItem
import kotlinx.coroutines.flow.first
import androidx.glance.appwidget.action.ActionCallback
import androidx.glance.appwidget.action.actionRunCallback
import androidx.glance.action.ActionParameters
import androidx.glance.action.actionParametersOf
import androidx.glance.appwidget.updateAll

class MemoListWidget : GlanceAppWidget() {
    override suspend fun provideGlance(context: Context, id: GlanceId) {
        val settings = SettingsManager(context)
        val fontSize = settings.widgetFontSize.first()
        val showCompleted = settings.showCompletedWidget.first()
        val url = settings.serverUrl.first()
        val token = settings.authToken.first()
        val memoId = settings.memoId.first()
        
        val repo = MemosRepository(url, token)
        val fetched = repo.fetchMemo(memoId)
        val rawItems = fetched ?: emptyList()
        
        val items = if (showCompleted) rawItems else rawItems.filter { it is MemoItem.Text || (it is MemoItem.Todo && !it.isChecked) }

        provideContent {
            val bgColor = Color(0xFF242424)
            val textColor = ColorProvider(Color.White)
            val dimTextColor = ColorProvider(Color(0xFF9A9996))
            
            Column(
                modifier = GlanceModifier
                    .fillMaxSize()
                    .background(bgColor)
                    .padding(8.dp)
            ) {
                Row(modifier = GlanceModifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically) {
                    Text(
                        text = "Memos ToDo",
                        modifier = GlanceModifier.defaultWeight().padding(start = 4.dp),
                        style = TextStyle(color = textColor, fontWeight = FontWeight.Bold)
                    )
                    Button(text = "Open", onClick = actionStartActivity<MainActivity>())
                }
                Spacer(modifier = GlanceModifier.height(8.dp))
                LazyColumn(modifier = GlanceModifier.fillMaxSize()) {
                    items(items) { item ->
                        when (item) {
                            is MemoItem.Todo -> {
                                Row(
                                    modifier = GlanceModifier.fillMaxWidth().padding(vertical = 4.dp),
                                    verticalAlignment = Alignment.CenterVertically
                                ) {
                                    val rawIndex = rawItems.indexOf(item)
                                    CheckBox(
                                        checked = item.isChecked, 
                                        onCheckedChange = actionRunCallback<ToggleTaskAction>(
                                            actionParametersOf(
                                                ToggleTaskAction.taskIndexKey to rawIndex,
                                                ToggleTaskAction.taskCheckedKey to !item.isChecked
                                            )
                                        )
                                    )
                                    val style = if (item.isChecked) {
                                        TextStyle(fontSize = fontSize.sp, color = dimTextColor, textDecoration = TextDecoration.LineThrough)
                                    } else {
                                        TextStyle(fontSize = fontSize.sp, color = textColor)
                                    }
                                    Text(text = item.text, style = style)
                                }
                            }
                            is MemoItem.Text -> {
                                Text(
                                    text = item.text,
                                    modifier = GlanceModifier.padding(start = 32.dp, top = 4.dp, bottom = 4.dp),
                                    style = TextStyle(fontSize = fontSize.sp, color = textColor)
                                )
                            }
                        }
                    }
                }
            }
        }
    }
}

class MemoListWidgetReceiver : GlanceAppWidgetReceiver() {
    override val glanceAppWidget: GlanceAppWidget = MemoListWidget()
}

class ToggleTaskAction : ActionCallback {
    override suspend fun onAction(
        context: Context,
        glanceId: GlanceId,
        parameters: ActionParameters
    ) {
        val taskIndex = parameters[taskIndexKey] ?: return
        val isChecked = parameters[taskCheckedKey] ?: return

        // 1. Optimistic update: instantly update Glance state if needed
        androidx.glance.appwidget.state.updateAppWidgetState(context, glanceId) { prefs ->
            // Glance natively toggles the checkbox UI optimistically on 'onCheckedChange' 
            // before this callback is even fired. However, if provideGlance runs 
            // before the server returns the new data, it will revert.
            // By deferring updateAll until AFTER the server returns, we prevent the bounce.
        }

        val settings = SettingsManager(context)
        val url = settings.serverUrl.first()
        val token = settings.authToken.first()
        val memoId = settings.memoId.first()

        val repo = MemosRepository(url, token)
        val fetched = repo.fetchMemo(memoId) ?: return
        val newList = fetched.toMutableList()

        if (taskIndex >= 0 && taskIndex < newList.size) {
            val item = newList[taskIndex]
            if (item is MemoItem.Todo) {
                // 2. Perform the actual data update
                newList[taskIndex] = item.copy(isChecked = isChecked)
                repo.updateMemo(memoId, newList)
                
                // 3. Force re-render widgets AFTER the server has accepted the new state
                MemoListWidget().updateAll(context)
                TaskBarWidget().updateAll(context)
            }
        }
    }

    companion object {
        val taskIndexKey = ActionParameters.Key<Int>("taskIndex")
        val taskCheckedKey = ActionParameters.Key<Boolean>("taskChecked")
    }
}
