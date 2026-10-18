package com.fbneo.android

import android.app.Activity
import android.app.AlertDialog
import android.graphics.*
import android.media.AudioAttributes
import android.media.AudioFormat
import android.media.AudioTrack
import android.opengl.GLES20
import android.opengl.GLSurfaceView
import android.os.Bundle
import android.view.MotionEvent
import android.view.View
import java.nio.ByteBuffer
import java.nio.ByteOrder
import java.nio.FloatBuffer
import javax.microedition.khronos.egl.EGLConfig
import javax.microedition.khronos.opengles.GL10

class GameActivity : Activity() {

    private var emuThread: Thread? = null
    @Volatile private var running = false
    private lateinit var glView: GLSurfaceView
    private lateinit var renderer: Renderer
    private var audio: AudioTrack? = null
    private val consoleLines = StringBuilder()

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        renderer = Renderer()
        glView = GLSurfaceView(this).apply {
            setEGLContextClientVersion(2)
            setRenderer(renderer)
            renderMode = GLSurfaceView.RENDERMODE_WHEN_DIRTY
        }

        val frame = android.widget.FrameLayout(this)
        frame.addView(glView)

        val pad = PadView(this)
        frame.addView(pad)

        val menu = android.widget.LinearLayout(this).apply {
            orientation = android.widget.LinearLayout.HORIZONTAL
        }
        fun btn(label: String, onClick: () -> Unit): android.widget.Button =
            android.widget.Button(this).apply {
                text = label; textSize = 12f
                setOnClickListener { onClick() }
                menu.addView(this, android.widget.LinearLayout.LayoutParams(0, -2, 1f))
            }
        btn("LUA") { luaMenu() }
        btn("SAVE") { Fbneo.nativeSaveState(autoStatePath()); toast("Saved") }
        btn("LOAD") { Fbneo.nativeLoadState(autoStatePath()); toast("Loaded") }
        btn("RESET") { Fbneo.nativeReset() }
        btn("FAST") { fast = !fast; Fbneo.nativeSetFast(if (fast) 1 else 0); toast(if (fast) "Fast ON" else "Fast OFF") }
        btn("EXIT") { finish() }

        val root = android.widget.LinearLayout(this).apply {
            orientation = android.widget.LinearLayout.VERTICAL
            addView(menu)
            addView(frame, android.widget.LinearLayout.LayoutParams(-1, 0, 1f))
        }
        setContentView(root)

        Fbneo.consoleListener = { line ->
            synchronized(consoleLines) {
                consoleLines.append(line).append('\n')
                if (consoleLines.length > 20000) consoleLines.delete(0, 10000)
            }
        }

        startAudio()
    }

    private var fast = false
    private fun toast(s: String) = runOnUiThread { android.widget.Toast.makeText(this, s, android.widget.Toast.LENGTH_SHORT).show() }
    private fun files() = getExternalFilesDir(null)!!.absolutePath
    private fun autoStatePath() = "${files()}/savestates/manual.fs"

    private fun luaMenu() {
        val dir = java.io.File(files(), "scripts")
        dir.mkdirs()
        val scripts = dir.listFiles { f -> f.extension == "lua" }?.map { it.name } ?: emptyList()
        val items = mutableListOf<String>()
        if (Fbneo.nativeLuaRunning() == 1) items += "Stop running script"
        items += "Reload last script"
        items += "Script console"
        items += "--- scripts in /scripts ---"
        items += scripts

        AlertDialog.Builder(this)
            .setTitle("Lua Scripting")
            .setItems(items.toTypedArray()) { _, which ->
                when {
                    items[which].startsWith("Stop") -> Fbneo.nativeLuaStop()
                    items[which].startsWith("Reload") -> Fbneo.nativeLuaReload()
                    items[which].startsWith("Script console") -> showConsole()
                    items[which].startsWith("---") -> {}
                    else -> {
                        val p = "${dir.absolutePath}/${items[which]}"
                        val r = Fbneo.nativeLuaLoad(p)
                        toast(if (r == 1) "Script started" else "Script error (see console)")
                        showConsole()
                    }
                }
            }
            .show()
    }

    private fun showConsole() {
        val tv = android.widget.TextView(this).apply {
            typeface = android.graphics.Typeface.MONOSPACE
            textSize = 11f
            setTextColor(Color.WHITE)
            setBackgroundColor(Color.BLACK)
            setPadding(16, 16, 16, 16)
            text = synchronized(consoleLines) { consoleLines.toString() }
        }
        val scroll = android.widget.ScrollView(this)
        scroll.addView(tv)
        AlertDialog.Builder(this).setTitle("Lua console").setView(scroll)
            .setPositiveButton("Close", null).show()
    }

    private fun startAudio() {
        val rate = Fbneo.nativeAudioRate()
        val frames = Fbneo.nativeAudioFrames()
        val minBuf = AudioTrack.getMinBufferSize(rate, AudioFormat.CHANNEL_OUT_STEREO, AudioFormat.ENCODING_PCM_16BIT)
        audio = AudioTrack.Builder()
            .setAudioAttributes(AudioAttributes.Builder().setUsage(AudioAttributes.USAGE_GAME).setContentType(AudioAttributes.CONTENT_TYPE_SONIFICATION).build())
            .setAudioFormat(AudioFormat.Builder().setSampleRate(rate).setEncoding(AudioFormat.ENCODING_PCM_16BIT).setChannelMask(AudioFormat.CHANNEL_OUT_STEREO).build())
            .setBufferSizeInBytes(minBuf.coerceAtLeast(frames * 4 * 8))
            .setTransferMode(AudioTrack.MODE_STREAM)
            .build()
        audio?.play()
        Thread {
            val arr = ByteArray(frames * 4)
            val buf = ByteBuffer.allocateDirect(frames * 4).order(ByteOrder.nativeOrder())
            while (true) {
                val a = audio ?: break
                buf.clear()
                Fbneo.nativeAudioPull(buf, frames)
                buf.get(arr)
                a.write(arr, 0, frames * 4)
            }
        }.start()
    }

    override fun onResume() {
        super.onResume()
        glView.onResume()
        running = true
        emuThread = Thread {
            val fps100 = Fbneo.nativeGetFps100().coerceAtLeast(1000)
            val frameNs = 1_000_000_000L * 100 / fps100
            while (running) {
                val t0 = System.nanoTime()
                Fbneo.nativeRunFrame()
                renderer.frameReady = true
                glView.requestRender()
                val dt = System.nanoTime() - t0
                val sleep = (frameNs - dt) / 1_000_000
                if (sleep > 0 && !fast) Thread.sleep(sleep)
            }
        }.apply { start() }
    }

    override fun onPause() {
        running = false
        emuThread?.join(500)
        glView.onPause()
        super.onPause()
    }

    override fun onDestroy() {
        Fbneo.nativeUnloadGame()
        audio?.stop(); audio?.release(); audio = null
        super.onDestroy()
    }

    inner class PadView(ctx: android.content.Context) : View(ctx) {
        private val paint = Paint(Paint.ANTI_ALIAS_FLAG)
        private data class Btn(val id: Int, val cx: Float, val cy: Float, val r: Float, val label: String)
        private var buttons: List<Btn> = emptyList()
        private val touchMap = HashMap<Int, Int>()

        override fun onSizeChanged(w: Int, h: Int, ow: Int, oh: Int) {
            val u = w / 100f
            buttons = listOf(
                Btn(2, 12*u, 62*h/100f, 7*u, "<"), Btn(3, 30*u, 62*h/100f, 7*u, ">"),
                Btn(0, 21*u, 48*h/100f, 7*u, "^"), Btn(1, 21*u, 76*h/100f, 7*u, "v"),
                Btn(4, 78*u, 62*h/100f, 8*u, "A"), Btn(5, 90*u, 66*h/100f, 8*u, "B"),
                Btn(6, 84*u, 84*h/100f, 8*u, "C"),
                Btn(7, 66*u, 74*h/100f, 6*u, "X"), Btn(8, 74*u, 88*h/100f, 6*u, "Y"), Btn(9, 88*u, 88*h/100f, 6*u, "Z"),
                Btn(20, 4*u, 6*h/100f, 5*u, "COIN"), Btn(21, 96*u, 6*h/100f, 5*u, "START")
            )
        }

        override fun onDraw(c: Canvas) {
            for (b in buttons) {
                paint.color = if (touchMap.values.contains(b.id)) 0xAAFFFFFF.toInt() else 0x44FFFFFF
                paint.style = Paint.Style.FILL
                c.drawCircle(b.cx, b.cy, b.r, paint)
                paint.color = Color.WHITE; paint.style = Paint.Style.STROKE; paint.strokeWidth = 2f
                c.drawCircle(b.cx, b.cy, b.r, paint)
                paint.style = Paint.Style.FILL; paint.textSize = b.r * 0.45f; paint.textAlign = Paint.Align.CENTER
                val baseline = b.cy - (paint.descent() + paint.ascent()) / 2
                c.drawText(b.label, b.cx, baseline, paint)
            }
        }

        override fun onTouchEvent(e: MotionEvent): Boolean {
            when (e.actionMasked) {
                MotionEvent.ACTION_DOWN, MotionEvent.ACTION_POINTER_DOWN -> {
                    val i = e.actionIndex
                    val b = buttons.minByOrNull { (e.getX(i) - it.cx) * (e.getX(i) - it.cx) + (e.getY(i) - it.cy) * (e.getY(i) - it.cy) }
                    if (b != null && (e.getX(i) - b.cx) * (e.getX(i) - b.cx) + (e.getY(i) - b.cy) * (e.getY(i) - b.cy) < b.r * b.r * 1.5f) {
                        touchMap[e.getPointerId(i)] = b.id
                        Fbneo.nativeSetInput(1, b.id, 1)
                        invalidate()
                    }
                }
                MotionEvent.ACTION_UP, MotionEvent.ACTION_POINTER_UP, MotionEvent.ACTION_CANCEL -> {
                    val i = e.actionIndex
                    val id = touchMap.remove(e.getPointerId(i))
                    if (id != null) { Fbneo.nativeSetInput(1, id, 0); invalidate() }
                    if (e.actionMasked == MotionEvent.ACTION_CANCEL) {
                        for ((_, cid) in touchMap) Fbneo.nativeSetInput(1, cid, 0)
                        touchMap.clear(); invalidate()
                    }
                }
            }
            return true
        }
    }

    inner class Renderer : GLSurfaceView.Renderer {
        @Volatile var frameReady = false
        private var texId = 0
        private var prog = 0
        private var texW = 0
        private var texH = 0
        private var px: ByteBuffer? = null

        private val vsrc = "attribute vec4 aPos; attribute vec2 aUV; varying vec2 vUV;" +
                "void main(){ gl_Position = aPos; vUV = aUV; }"
        private val fsrc = "precision mediump float; varying vec2 vUV; uniform sampler2D uTex;" +
                "void main(){ gl_FragColor = texture2D(uTex, vUV); }"

        private fun compile(type: Int, src: String): Int {
            val s = GLES20.glCreateShader(type); GLES20.glShaderSource(s, src); GLES20.glCompileShader(s); return s
        }

        override fun onSurfaceCreated(gl: GL10?, cfg: EGLConfig?) {
            val vs = compile(GLES20.GL_VERTEX_SHADER, vsrc)
            val fs = compile(GLES20.GL_FRAGMENT_SHADER, fsrc)
            prog = GLES20.glCreateProgram()
            GLES20.glAttachShader(prog, vs); GLES20.glAttachShader(prog, fs); GLES20.glLinkProgram(prog)
            val ids = IntArray(1); GLES20.glGenTextures(1, ids, 0); texId = ids[0]
            GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, texId)
            GLES20.glTexParameteri(GLES20.GL_TEXTURE_2D, GLES20.GL_TEXTURE_MIN_FILTER, GLES20.GL_LINEAR)
            GLES20.glTexParameteri(GLES20.GL_TEXTURE_2D, GLES20.GL_TEXTURE_MAG_FILTER, GLES20.GL_LINEAR)
            GLES20.glTexParameteri(GLES20.GL_TEXTURE_2D, GLES20.GL_TEXTURE_WRAP_S, GLES20.GL_CLAMP_TO_EDGE)
            GLES20.glTexParameteri(GLES20.GL_TEXTURE_2D, GLES20.GL_TEXTURE_WRAP_T, GLES20.GL_CLAMP_TO_EDGE)
        }

        override fun onSurfaceChanged(gl: GL10?, w: Int, h: Int) {
            val gw = texW.coerceAtLeast(1); val gh = texH.coerceAtLeast(1)
            var vw = w; var vh = h
            if (w * gh > h * gw) vw = h * gw / gh else vh = w * gh / gw
            GLES20.glViewport((w - vw) / 2, (h - vh) / 2, vw, vh)
        }

        override fun onDrawFrame(gl: GL10?) {
            if (texW <= 0) {
                texW = Fbneo.nativeVideoWidth(); texH = Fbneo.nativeVideoHeight()
                if (texW <= 0) return
                px = ByteBuffer.allocateDirect(texW * texH * 4).order(ByteOrder.nativeOrder())
                GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, texId)
                GLES20.glTexImage2D(GLES20.GL_TEXTURE_2D, 0, GLES20.GL_RGBA, texW, texH, 0,
                    GLES20.GL_RGBA, GLES20.GL_UNSIGNED_BYTE, null)
            }
            val buf = px ?: return
            buf.clear()
            Fbneo.nativeCopyFrame(buf)

            GLES20.glClear(GLES20.GL_COLOR_BUFFER_BIT)
            GLES20.glUseProgram(prog)
            GLES20.glActiveTexture(GLES20.GL_TEXTURE0)
            GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, texId)
            GLES20.glUniform1i(GLES20.glGetUniformLocation(prog, "uTex"), 0)
            buf.position(0)
            GLES20.glTexSubImage2D(GLES20.GL_TEXTURE_2D, 0, 0, 0, texW, texH,
                GLES20.GL_RGBA, GLES20.GL_UNSIGNED_BYTE, buf)

            val quad = floatArrayOf(-1f, -1f, 0f, 1f, 1f, -1f, 1f, 1f, -1f, 1f, 0f, 0f, 1f, 1f, 1f, 0f)
            val fb: FloatBuffer = ByteBuffer.allocateDirect(quad.size * 4).order(ByteOrder.nativeOrder()).asFloatBuffer().put(quad).also { it.position(0) }
            val aPos = GLES20.glGetAttribLocation(prog, "aPos")
            val aUV = GLES20.glGetAttribLocation(prog, "aUV")
            GLES20.glEnableVertexAttribArray(aPos)
            GLES20.glVertexAttribPointer(aPos, 2, GLES20.GL_FLOAT, false, 16, fb)
            GLES20.glEnableVertexAttribArray(aUV)
            fb.position(2)
            GLES20.glVertexAttribPointer(aUV, 2, GLES20.GL_FLOAT, false, 16, fb)
            GLES20.glDrawArrays(GLES20.GL_TRIANGLE_STRIP, 0, 4)
        }
    }
}
