// SPDX-License-Identifier: GPL-3.0-or-later
// Conformance TEST FIXTURE for the webOS plugin runtime. Not a content provider: it serves fixed data
// from its own resources so requiresResources, settings screens and dialogs can be tested end to end.
// Written against CloudStream's plugin API exactly as third-party plugins are.
package org.csweb.conformance

import android.app.AlertDialog
import android.content.Context
import android.content.res.Resources
import android.view.LayoutInflater
import android.view.View
import android.widget.CheckBox
import android.widget.EditText
import com.lagradost.cloudstream3.LoadResponse
import com.lagradost.cloudstream3.MainAPI
import com.lagradost.cloudstream3.SearchResponse
import com.lagradost.cloudstream3.SubtitleFile
import com.lagradost.cloudstream3.TvType
import com.lagradost.cloudstream3.newMovieLoadResponse
import com.lagradost.cloudstream3.newMovieSearchResponse
import com.lagradost.cloudstream3.plugins.Plugin
import com.lagradost.cloudstream3.utils.ExtractorLink
import com.lagradost.cloudstream3.utils.newExtractorLink

private const val PKG = "org.csweb.conformance"
private const val PREFS = "csweb_conformance"

private fun Resources.id(name: String, type: String): Int {
    val id = getIdentifier(name, type, PKG)
    if (id == 0) throw Resources.NotFoundException("$type/$name")
    return id
}

class ConformancePlugin : Plugin() {
    override fun load(context: Context) {
        val res = resources ?: throw IllegalStateException("requiresResources is set but resources is null")
        val prefs = context.getSharedPreferences(PREFS, Context.MODE_PRIVATE)
        registerMainAPI(ConformanceProvider(res, prefs.getString("name", null), prefs.getBoolean("extra", false)))
        openSettings = { ctx -> showSettings(ctx, res) }
    }

    private fun showSettings(ctx: Context, res: Resources) {
        val prefs = ctx.getSharedPreferences(PREFS, Context.MODE_PRIVATE)
        val view: View = LayoutInflater.from(ctx).inflate(res.getLayout(res.id("settings", "layout")), null, false)
        val name = view.findViewById<EditText>(res.id("name_field", "id"))
        val extra = view.findViewById<CheckBox>(res.id("enabled_box", "id"))
        name.setText(prefs.getString("name", ""))
        extra.isChecked = prefs.getBoolean("extra", false)
        AlertDialog.Builder(ctx)
            .setTitle(res.getString(res.id("settings_title", "string")))
            .setView(view)
            .setPositiveButton(res.getString(res.id("save", "string"))) { _, _ ->
                prefs.edit().putString("name", name.text.toString()).putBoolean("extra", extra.isChecked).apply()
            }
            .setNegativeButton(res.getString(res.id("cancel", "string")), null)
            .show()
    }
}

class ConformanceProvider(
    private val res: Resources,
    private val displayName: String?,
    private val extra: Boolean,
) : MainAPI() {
    override var name = displayName ?: res.getString(res.id("provider_name", "string"))
    override var mainUrl = "https://conformance.invalid"
    override var lang = "en"
    override val supportedTypes = setOf(TvType.Others)

    override suspend fun search(query: String): List<SearchResponse> {
        val titles = res.getStringArray(res.id("titles", "array")).toList()
        val shown = if (extra) titles else titles.take(2)
        val color = res.getColor(res.id("accent", "color"), null)
        val padding = res.getDimensionPixelSize(res.id("padding", "dimen"))
        return shown.filter { it.contains(query, ignoreCase = true) || query == "*" }.mapIndexed { i, t ->
            newMovieSearchResponse(t, "$mainUrl/item/$i?color=${Integer.toHexString(color)}&padding=$padding", TvType.Others)
        }
    }

    override suspend fun load(url: String): LoadResponse {
        val i = url.substringAfter("/item/").substringBefore("?").toInt()
        val title = res.getStringArray(res.id("titles", "array"))[i]
        return newMovieLoadResponse(title, url, TvType.Others, "$i") {
            plot = res.getString(res.id("plot", "string"))
        }
    }

    override suspend fun loadLinks(
        data: String,
        isCasting: Boolean,
        subtitleCallback: (SubtitleFile) -> Unit,
        callback: (ExtractorLink) -> Unit,
    ): Boolean {
        // A public-domain LibriVox recording; the runtime never fetches it, the app's player would.
        callback(newExtractorLink(name, name,
            "https://www.archive.org/download/pride_and_prejudice_librivox/prideandprejudice_01-03_austen.mp3"))
        return true
    }
}
