@file:OptIn(kotlinx.cinterop.ExperimentalForeignApi::class)

import aamp.*
import kotlinx.cinterop.*
import platform.posix.*
import kotlin.native.concurrent.*

fun main() {
    println("================================================")
    println("Starting AAMPCLI On Kotlin")
	val worker = Worker.start() // start new thread
    val future = worker.execute( TransferMode.SAFE, { "cli" } )
    {
        input ->
            println("$input")
			while( true )
			{
				println( "enter DASH or HLS locator" )
				val url = readlnOrNull()
				kmp_tune( url )
			}
    }
    kmp_init()
}
