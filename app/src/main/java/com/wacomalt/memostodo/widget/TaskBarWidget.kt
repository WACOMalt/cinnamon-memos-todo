package com.wacomalt.memostodo.widget

import android.content.Context
import androidx.glance.GlanceId
import androidx.glance.GlanceModifier
import androidx.glance.appwidget.GlanceAppWidget
import androidx.glance.appwidget.GlanceAppWidgetReceiver
import androidx.glance.appwidget.provideContent
import androidx.glance.background
import androidx.glance.layout.*
import androidx.glance.text.Text
import androidx.glance.text.TextStyle
import androidx.glance.action.actionStartActivity
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.compose.ui.graphics.Color
import androidx.glance.unit.ColorProvider
import com.wacomalt.memostodo.MainActivity
import com.wacomalt.memostodo.SettingsManager
import com.wacomalt.memostodo.api.MemosRepository
import com.wacomalt.memostodo.model.MemoItem
import kotlinx.coroutines.flow.first

class TaskBarWidget : GlanceAppWidget() {
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
        
        var currentTask = "Loading tasks..."
        if (rawItems.isNotEmpty()) {
            val validItems = rawItems.filter { 
                if (!showCompleted && it is MemoItem.Todo && it.isChecked) false else true 
            }
            // Find first uncompleted todo, else first valid item
            val firstTodo = validItems.filterIsInstance<MemoItem.Todo>().firstOrNull { !it.isChecked }
            if (firstTodo != null) {
                currentTask = "☐ ${firstTodo.text}"
            } else if (validItems.isNotEmpty()) {
                val first = validItems.first()
                if (first is MemoItem.Todo) {
                    currentTask = "${if(first.isChecked) "☑" else "☐"} ${first.text}"
                } else if (first is MemoItem.Text) {
                    currentTask = first.text
                }
            } else {
                currentTask = "All tasks completed!"
            }
        }

        provideContent {
            val textColor = ColorProvider(Color.White)
            Box(
                modifier = GlanceModifier
                    .fillMaxSize()
                    .background(Color(0xFF242424))
                    .padding(4.dp),
                contentAlignment = Alignment.CenterStart
            ) {
                Row(
                    modifier = GlanceModifier.fillMaxSize(),
                    verticalAlignment = Alignment.CenterVertically
                ) {
                    Text(
                        text = currentTask,
                        modifier = GlanceModifier.defaultWeight().padding(start = 8.dp),
                        style = TextStyle(fontSize = fontSize.sp, color = textColor)
                    )
                    androidx.glance.Button(text = ">", onClick = actionStartActivity<MainActivity>())
                }
            }
        }
    }
}

class TaskBarWidgetReceiver : GlanceAppWidgetReceiver() {
    override val glanceAppWidget: GlanceAppWidget = TaskBarWidget()
}
