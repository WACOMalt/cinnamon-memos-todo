package com.wacomalt.memostodo.api

import com.wacomalt.memostodo.model.MemoItem
import com.wacomalt.memostodo.model.PatchMemoRequest
import okhttp3.OkHttpClient
import okhttp3.logging.HttpLoggingInterceptor
import retrofit2.Retrofit
import retrofit2.converter.gson.GsonConverterFactory

class MemosRepository(private var baseUrl: String, private val authToken: String) {

    private val apiService: MemosApiService by lazy {
        val logging = HttpLoggingInterceptor { message ->
            android.util.Log.e("OkHttp", message)
        }.apply {
            level = HttpLoggingInterceptor.Level.BODY
        }
        val client = OkHttpClient.Builder()
            .addInterceptor(logging)
            .build()

        Retrofit.Builder()
            .baseUrl(baseUrl.ensureTrailingSlash())
            .addConverterFactory(GsonConverterFactory.create())
            .client(client)
            .build()
            .create(MemosApiService::class.java)
    }

    suspend fun fetchMemo(memoId: String): List<MemoItem>? {
        return try {
            if (authToken.isEmpty()) return null
            val response = apiService.getMemo(memoId, "Bearer $authToken")
            if (response.isSuccessful) {
                parseContent(response.body()?.content ?: "")
            } else {
                null
            }
        } catch (e: Exception) {
            null
        }
    }

    suspend fun updateMemo(memoId: String, items: List<MemoItem>): Boolean {
        val content = serializeItems(items)
        return try {
            android.util.Log.d("MemosRepository", "Updating memo $memoId with content: ${content.replace("\n", "|")}")
            val response = apiService.patchMemo(memoId, "Bearer $authToken", PatchMemoRequest(content))
            if (!response.isSuccessful) {
                android.util.Log.e("MemosRepository", "Update failed: ${response.code()} ${response.message()} ${response.errorBody()?.string()}")
            } else {
                android.util.Log.d("MemosRepository", "Update success: ${response.code()}")
            }
            response.isSuccessful
        } catch (e: Exception) {
            android.util.Log.e("MemosRepository", "Update exception", e)
            false
        }
    }

    fun parseContent(content: String): List<MemoItem> {
        val lines = content.split("\n")
        val items = mutableListOf<MemoItem>()
        for (line in lines) {
            val lineTrim = line.trim()
            if (lineTrim.isEmpty()) continue
            if (lineTrim.startsWith("☐ ")) {
                items.add(MemoItem.Todo(lineTrim.substring(2), false))
            } else if (lineTrim.startsWith("☑ ")) {
                items.add(MemoItem.Todo(lineTrim.substring(2), true))
            } else {
                items.add(MemoItem.Text(lineTrim))
            }
        }
        return items
    }

    fun serializeItems(items: List<MemoItem>): String {
        return items.joinToString("\n") {
            when (it) {
                is MemoItem.Todo -> if (it.isChecked) "☑ ${it.text}" else "☐ ${it.text}"
                is MemoItem.Text -> it.text
            }
        }
    }

    private fun String.ensureTrailingSlash() = if (endsWith("/")) this else "$this/"
}
