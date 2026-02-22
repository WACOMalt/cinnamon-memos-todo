package com.wacomalt.memostodo.model

import com.google.gson.annotations.SerializedName

data class Memo(
    val id: String,
    val content: String,
    val rowStatus: String? = null,
    val createTime: String? = null,
    val updateTime: String? = null
)

data class MemoResponse(
    val memo: Memo? = null,
    val content: String? = null // Some versions might return just content
)

data class PatchMemoRequest(
    val content: String
)

sealed class MemoItem {
    data class Todo(val text: String, val isChecked: Boolean) : MemoItem()
    data class Text(val text: String) : MemoItem()
}
