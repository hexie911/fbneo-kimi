package com.fbneo.android

object Fbneo {
    init { System.loadLibrary("fbneo") }

    external fun nativeInit(filesDir: String)
    external fun nativeSetConsoleHandler(cls: Class<*>, method: String)
    external fun nativeGetDriverCount(): Int
    external fun nativeGetDriverName(i: Int): String
    external fun nativeGetDriverZip(i: Int): String
    external fun nativeLoadGame(i: Int): Int
    external fun nativeUnloadGame()
    external fun nativeGetFps100(): Int
    external fun nativeRunFrame()
    external fun nativeGetLastError(): String
    external fun nativeVideoWidth(): Int
    external fun nativeVideoHeight(): Int
    external fun nativeCopyFrame(buf: java.nio.ByteBuffer)
    external fun nativeAudioRate(): Int
    external fun nativeAudioFrames(): Int
    external fun nativeAudioPull(buf: java.nio.ByteBuffer, frames: Int): Int
    external fun nativeSetInput(player: Int, id: Int, down: Int)
    external fun nativeReset()
    external fun nativeSaveState(path: String): Int
    external fun nativeLoadState(path: String): Int
    external fun nativeLuaLoad(path: String): Int
    external fun nativeLuaStop()
    external fun nativeLuaReload()
    external fun nativeLuaRunning(): Int
    external fun nativeSetFast(on: Int)

    var consoleListener: ((String) -> Unit)? = null

    @JvmStatic
    fun onConsoleLine(line: String) {
        consoleListener?.invoke(line.trimEnd())
    }
}
