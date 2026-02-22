package com.wacomalt.memostodo.widget

import android.content.Context
import androidx.glance.GlanceId
import androidx.glance.GlanceModifier
import androidx.glance.appwidget.GlanceAppWidget
import androidx.glance.appwidget.GlanceAppWidgetReceiver
import androidx.glance.appwidget.lazy.LazyColumn
import androidx.glance.appwidget.lazy.items
import androidx.glance.appwidget.provideContent
import androidx.glance.layout.*
import androidx.glance.text.Text
import androidx.glance.Button
import androidx.glance.action.actionStartActivity
import androidx.glance.appwidget.CheckBox
import androidx.glance.text.TextStyle
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.wacomalt.memostodo.MainActivity
import com.wacomalt.memostodo.SettingsManager
import com.wacomalt.memostodo.model.MemoItem
import kotlinx.coroutines.flow.first

class MemoListWidget : GlanceAppWidget() {
    override suspend fun provideGlance(context: Context, id: GlanceId) {
        val settings = SettingsManager(context)
        val fontSize = settings.widgetFontSize.first()
        val showCompleted = settings.showCompletedWidget.first()
        
        val rawItems = listOf(
            MemoItem.Todo("Task 1", false),
            MemoItem.Todo("Task 2", true),
            MemoItem.Text("Some note content")
        )
        
        val items = if (showCompleted) rawItems else rawItems.filter { it is MemoItem.Text || (it is MemoItem.Todo && !it.isChecked) }

        provideContent {
            Column(
                modifier = GlanceModifier
                    .fillMaxSize()
                    .padding(8.dp)
            ) {
                Row(modifier = GlanceModifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically) {
                    Text(text = "Memos ToDo", modifier = GlanceModifier.defaultWeight())
                    Button(text = "Open", onClick = actionStartActivity<MainActivity>())
                }
                Spacer(modifier = GlanceModifier.height(8.dp))
                LazyColumn(modifier = GlanceModifier.fillMaxSize()) {
                    items(items) { item ->
                        when (item) {
                            is MemoItem.Todo -> {
                                Row(verticalAlignment = Alignment.CenterVertically) {
                                    CheckBox(checked = item.isChecked, onCheckedChange = null)
                                    Text(text = item.text, style = TextStyle(fontSize = fontSize.sp))
                                }
                            }
                            is MemoItem.Text -> {
                                Text(text = item.text, modifier = GlanceModifier.padding(start = 24.dp), style = TextStyle(fontSize = fontSize.sp))
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
