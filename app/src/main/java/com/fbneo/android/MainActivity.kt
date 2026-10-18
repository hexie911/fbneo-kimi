package com.fbneo.android

import android.app.Activity
import android.content.Intent
import android.graphics.Color
import android.os.Bundle
import android.view.ViewGroup
import android.widget.*

class MainActivity : Activity() {

    private lateinit var romsDir: java.io.File

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        val files = getExternalFilesDir(null)!!.absolutePath
        romsDir = java.io.File(files, "roms")
        java.io.File(files, "scripts").mkdirs()

        Fbneo.nativeInit(files)
        Fbneo.nativeSetConsoleHandler(Fbneo::class.java, "onConsoleLine")

        val root = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            setBackgroundColor(Color.rgb(0x12, 0x14, 0x1A))
            setPadding(24, 24, 24, 24)
        }

        val title = TextView(this).apply {
            text = "FBNeo Android"
            textSize = 26f
            setTextColor(Color.WHITE)
            setPadding(0, 0, 0, 8)
        }

        val hint = TextView(this).apply {
            text = "Put game .zip files in:\n${romsDir.absolutePath}\n\nPut Lua scripts (e.g. 3rd_training.lua) in:\n${java.io.File(files, "scripts").absolutePath}"
            setTextColor(Color.LTGRAY)
            setPadding(0, 0, 0, 16)
        }

        val search = EditText(this).apply { hint = "Search games..."; setTextColor(Color.WHITE) }

        val list = ListView(this)
        val all = mutableListOf<Triple<Int, String, Boolean>>()
        val adapter = object : ArrayAdapter<String>(this, android.R.layout.simple_list_item_1, mutableListOf()) {
            override fun getView(position: Int, convertView: android.view.View?, parent: ViewGroup): android.view.View {
                val v = super.getView(position, convertView, parent)
                (v as TextView).setTextColor(if (position < all.size && all[position].third) Color.WHITE else Color.DKGRAY)
                return v
            }
        }
        list.adapter = adapter

        fun refresh(filter: String) {
            all.clear()
            val zips = romsDir.listFiles { f -> f.extension == "zip" }?.map { it.nameWithoutExtension }?.toSet() ?: emptySet()
            for (i in 0 until Fbneo.nativeGetDriverCount()) {
                val zip = Fbneo.nativeGetDriverZip(i)
                if (zip in zips) all.add(Triple(i, Fbneo.nativeGetDriverName(i), true))
            }
            if (filter.isNotBlank()) all.removeAll { !it.second.contains(filter, true) }
            adapter.clear()
            adapter.addAll(all.map { it.second })
            adapter.notifyDataSetChanged()
        }

        search.addTextChangedListener(object : android.text.TextWatcher {
            override fun afterTextChanged(s: android.text.Editable?) = refresh(s?.toString() ?: "")
            override fun beforeTextChanged(s: CharSequence?, a: Int, b: Int, c: Int) {}
            override fun onTextChanged(s: CharSequence?, a: Int, b: Int, c: Int) {}
        })

        list.setOnItemClickListener { _, _, pos, _ ->
            val (idx, _, ok) = all[pos]
            if (!ok) return@setOnItemClickListener
            val err = Fbneo.nativeLoadGame(idx)
            if (err != 0) {
                Toast.makeText(this, "Load failed: ${Fbneo.nativeGetLastError()}", Toast.LENGTH_LONG).show()
            } else {
                startActivity(Intent(this, GameActivity::class.java))
            }
        }

        root.addView(title)
        root.addView(hint)
        root.addView(search)
        root.addView(list, LinearLayout.LayoutParams(-1, 0, 1f))
        setContentView(root)

        refresh("")
    }
}
