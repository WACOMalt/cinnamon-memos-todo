package com.wacomalt.memostodo.api

import com.wacomalt.memostodo.model.Memo
import com.wacomalt.memostodo.model.PatchMemoRequest
import retrofit2.http.Body
import retrofit2.http.GET
import retrofit2.http.Header
import retrofit2.http.PATCH
import retrofit2.http.Path

interface MemosApiService {
    @GET("api/v1/memos/{memoId}")
    suspend fun getMemo(
        @Path("memoId") memoId: String,
        @Header("Authorization") authToken: String
    ): retrofit2.Response<com.wacomalt.memostodo.model.Memo>

    @PATCH("api/v1/memos/{memoId}")
    suspend fun patchMemo(
        @Path("memoId") memoId: String,
        @Header("Authorization") authToken: String,
        @Body request: PatchMemoRequest
    ): retrofit2.Response<com.wacomalt.memostodo.model.Memo>
}
