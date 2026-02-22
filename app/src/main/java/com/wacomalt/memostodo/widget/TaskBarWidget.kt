package com.wacomalt.memostodo.widget

import android.content.Context
import androidx.glance.GlanceId
import androidx.glance.GlanceModifier
import androidx.glance.appwidget.GlanceAppWidget
import androidx.glance.appwidget.GlanceAppWidgetReceiver
import androidx.glance.appwidget.provideContent
import androidx.glance.layout.*
import androidx.glance.text.Text
import androidx.glance.text.TextStyle
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.glance.action.actionStartActivity
import com.wacomalt.memostodo.MainActivity
import com.wacomalt.memostodo.SettingsManager
import kotlinx.coroutines.flow.first

class TaskBarWidget : GlanceAppWidget() {
    override suspend fun provideGlance(context: Context, id: GlanceId) {
        val settings = SettingsManager(context)
        val fontSize = settings.widgetFontSize.first()
        
        val currentTask = "☐ Finish Android app plan"

        provideContent {
            Box(
                modifier = GlanceModifier
                    .fillMaxSize()
                    .padding(4.dp),
                contentAlignment = Alignment.CenterStart
            ) {
                Row(
                    modifier = GlanceModifier.fillMaxSize(),
                    verticalAlignment = Alignment.CenterVertically
                ) {
                    Text(
                        text = currentTask,
                        modifier = GlanceModifier.defaultWeight(),
                        style = TextStyle(fontSize = fontSize.sp)
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
