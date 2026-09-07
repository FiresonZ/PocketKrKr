/*
 * KrKr2 Engine Bridge — Android plugin.
 *
 * Bridges the KrKr2 C++ engine to Flutter Android:
 *   - getPlatformVersion
 *   - Surface zero-copy rendering (GPU path): the Kotlin side creates a
 *     Flutter-managed Surface (via TextureRegistry.SurfaceProducer) and hands
 *     the ANativeWindow to the engine via JNI (nativeSetSurface /
 *     nativeDetachSurface). The engine renders with EGL (ANGLE) and
 *     eglSwapBuffers delivers frames directly to Flutter, with no CPU readback.
 *   - Application Context pass-through (nativeSetApplicationContext) so engine
 *     C++ (AndroidUtils.cpp) can query storage paths without KR2Activity.
 *   - External storage permission (hasManageExternalStorage /
 *     requestManageExternalStorage) and SAF file picker (pickFile /
 *     resolveContentUri).
 *
 * Surface path aligned with local PocketKrKr (SurfaceProducer); permission /
 * SAF / context JNI aligned with upstream reAAAq/KrKr2-Next.
 *
 * The native library (libengine_api.so) is built by build/build_android.sh
 * and copied into the app's jniLibs/arm64-v8a/ — it contains both the engine
 * runtime and the JNI glue (engine_api_android_jni.cpp).
 *
 * NOTE: the legacy CPU readback path (createTexture / updateTextureRgba /
 * disposeTexture) relied on TextureRegistry.TextureEntry.copyPixelBuffer(),
 * removed from the modern Flutter Android embedding; those methods are kept
 * for Dart-side compatibility but return an explicit "not supported on Android"
 * error. The GPU path is the canonical way to get frames from the engine.
 */

package dev.pocketkrkr.flutter_engine_bridge

import android.app.Activity
import android.content.Intent
import android.net.Uri
import android.os.Build
import android.os.Environment
import android.provider.DocumentsContract
import android.provider.Settings
import android.util.Log
import android.view.Surface
import io.flutter.embedding.engine.plugins.FlutterPlugin
import io.flutter.embedding.engine.plugins.activity.ActivityAware
import io.flutter.embedding.engine.plugins.activity.ActivityPluginBinding
import io.flutter.plugin.common.MethodCall
import io.flutter.plugin.common.MethodChannel
import io.flutter.plugin.common.MethodChannel.MethodCallHandler
import io.flutter.plugin.common.MethodChannel.Result
import io.flutter.plugin.common.PluginRegistry
import io.flutter.view.TextureRegistry

class FlutterEngineBridgePlugin :
  FlutterPlugin,
  MethodCallHandler,
  ActivityAware,
  PluginRegistry.ActivityResultListener {

  private lateinit var channel: MethodChannel
  private var textureRegistry: TextureRegistry? = null
  private var binding: FlutterPlugin.FlutterPluginBinding? = null
  private var activity: Activity? = null
  private var activityBinding: ActivityPluginBinding? = null
  private var pendingPickResult: Result? = null

  private val surfaceProducers = mutableMapOf<Long, TextureRegistry.SurfaceProducer>()

  companion object {
    private const val PICK_FILE_REQUEST = 9001
    private const val TAG = "krkr2"

    init {
      try {
        System.loadLibrary("engine_api")
      } catch (e: UnsatisfiedLinkError) {
        Log.w(TAG, "engine_api native lib not found: ${e.message}")
      }
    }
  }

  private external fun nativeSetSurface(surface: Surface?, width: Int, height: Int)
  private external fun nativeDetachSurface()
  private external fun nativeSetApplicationContext(context: android.content.Context)

  override fun onAttachedToEngine(flutterPluginBinding: FlutterPlugin.FlutterPluginBinding) {
    binding = flutterPluginBinding
    textureRegistry = flutterPluginBinding.textureRegistry
    channel = MethodChannel(flutterPluginBinding.binaryMessenger, "flutter_engine_bridge")
    channel.setMethodCallHandler(this)

    // Pass Application Context to the native engine so C++ code (AndroidUtils.cpp)
    // can query storage paths / fonts without depending on KR2Activity being created.
    try {
      nativeSetApplicationContext(flutterPluginBinding.applicationContext)
    } catch (e: UnsatisfiedLinkError) {
      Log.w(TAG, "nativeSetApplicationContext not available: ${e.message}")
    }
  }

  override fun onDetachedFromEngine(flutterPluginBinding: FlutterPlugin.FlutterPluginBinding) {
    channel.setMethodCallHandler(null)
    for ((_, producer) in surfaceProducers) {
      producer.release()
    }
    surfaceProducers.clear()
    try {
      nativeDetachSurface()
    } catch (_: UnsatisfiedLinkError) {
    }
    textureRegistry = null
    binding = null
  }

  override fun onMethodCall(call: MethodCall, result: Result) {
    when (call.method) {
      "getPlatformVersion" -> result.success("Android ${Build.VERSION.RELEASE}")

      "hasManageExternalStorage" -> {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
          result.success(Environment.isExternalStorageManager())
        } else {
          result.success(true)
        }
      }

      "requestManageExternalStorage" -> {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
          val ctx = binding?.applicationContext
          if (ctx == null) {
            result.error("detached", "Flutter engine is detached", null)
            return
          }
          val intent = Intent(Settings.ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION)
            .setData(Uri.parse("package:${ctx.packageName}"))
            .addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
          try {
            ctx.startActivity(intent)
            result.success(true)
          } catch (_: Exception) {
            val fallbackIntent = Intent(Settings.ACTION_MANAGE_ALL_FILES_ACCESS_PERMISSION)
              .addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
            try {
              ctx.startActivity(fallbackIntent)
              result.success(true)
            } catch (e: Exception) {
              result.error(
                "permission",
                "Failed to open all-files access settings: ${e.message}",
                null,
              )
            }
          }
        } else {
          result.success(true)
        }
      }

      // --- Legacy RGBA readback (removed from the modern Android embedding) ---
      "createTexture", "updateTextureRgba", "disposeTexture", "notifyFrameAvailable" ->
        result.success(null)

      // --- Surface zero-copy (GPU path) ---
      "createSurfaceTexture" -> {
        val width = call.argument<Number>("width")?.toInt()
        val height = call.argument<Number>("height")?.toInt()
        if (width == null || height == null || width <= 0 || height <= 0) {
          result.error("invalid_args", "createSurfaceTexture requires width/height > 0", null)
          return
        }
        val registry = textureRegistry
        if (registry == null) {
          result.error("detached", "Flutter engine is detached", null)
          return
        }
        val producer = registry.createSurfaceProducer()
        producer.setSize(width, height)
        val textureId = producer.id()
        surfaceProducers[textureId] = producer
        val surface = producer.getSurface()
        try {
          nativeSetSurface(surface, width, height)
        } catch (e: UnsatisfiedLinkError) {
          Log.e(TAG, "nativeSetSurface not available: ${e.message}")
        }
        result.success(mapOf(
          "textureId" to textureId,
          "width" to width,
          "height" to height,
        ))
      }

      "resizeSurfaceTexture" -> {
        val textureId = call.argument<Number>("textureId")?.toLong()
        val width = call.argument<Number>("width")?.toInt()
        val height = call.argument<Number>("height")?.toInt()
        val producer = textureId?.let { surfaceProducers[it] }
        if (producer == null || width == null || height == null || width <= 0 || height <= 0) {
          result.error(
            "invalid_args",
            "resizeSurfaceTexture requires existing textureId and width/height > 0",
            null,
          )
          return
        }
        producer.setSize(width, height)
        val surface = producer.getSurface()
        try {
          nativeSetSurface(surface, width, height)
        } catch (e: UnsatisfiedLinkError) {
          Log.e(TAG, "nativeSetSurface not available: ${e.message}")
        }
        result.success(mapOf(
          "textureId" to textureId,
          "width" to width,
          "height" to height,
        ))
      }

      "disposeSurfaceTexture" -> {
        val textureId = call.argument<Number>("textureId")?.toLong()
        val producer = textureId?.let { surfaceProducers.remove(it) }
        if (producer != null) {
          try {
            nativeDetachSurface()
          } catch (_: UnsatisfiedLinkError) {
          }
          producer.release()
        }
        result.success(null)
      }

      "resolveContentUri" -> {
        val uriString = call.argument<String>("uri")
        if (uriString == null) {
          result.success(null)
          return
        }
        result.success(resolveRealPath(Uri.parse(uriString)))
      }

      "pickFile" -> {
        val act = activity
        if (act == null) {
          result.error("no_activity", "No activity available", null)
          return
        }
        if (pendingPickResult != null) {
          result.error("busy", "Another pick is in progress", null)
          return
        }
        pendingPickResult = result
        val intent = Intent(Intent.ACTION_OPEN_DOCUMENT).apply {
          addCategory(Intent.CATEGORY_OPENABLE)
          type = "*/*"
        }
        act.startActivityForResult(intent, PICK_FILE_REQUEST)
      }

      else -> result.notImplemented()
    }
  }

  private fun requireTextureRegistry(): TextureRegistry {
    return textureRegistry
      ?: throw IllegalStateException("flutter_engine_bridge plugin not attached to engine")
  }

  private fun resolveRealPath(uri: Uri): String? {
    val ctx = binding?.applicationContext ?: return null
    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.KITKAT &&
      DocumentsContract.isDocumentUri(ctx, uri)
    ) {
      val docId = DocumentsContract.getDocumentId(uri)
      if (uri.authority == "com.android.externalstorage.documents") {
        val parts = docId.split(":", limit = 2)
        if (parts.size == 2) {
          val storageType = parts[0]
          val relativePath = parts[1]
          val root = if (storageType.equals("primary", ignoreCase = true)) {
            Environment.getExternalStorageDirectory().absolutePath
          } else {
            "/storage/$storageType"
          }
          return "$root/$relativePath"
        }
      }
    }
    try {
      ctx.contentResolver.query(uri, arrayOf("_data"), null, null, null)?.use { cursor ->
        if (cursor.moveToFirst()) {
          val idx = cursor.getColumnIndex("_data")
          if (idx >= 0) return cursor.getString(idx)
        }
      }
    } catch (_: Exception) {
    }
    return null
  }

  // --- ActivityAware (for SAF file picker) ---

  override fun onAttachedToActivity(binding: ActivityPluginBinding) {
    activity = binding.activity
    activityBinding = binding
    binding.addActivityResultListener(this)
  }

  override fun onDetachedFromActivity() {
    activityBinding?.removeActivityResultListener(this)
    activity = null
    activityBinding = null
  }

  override fun onReattachedToActivityForConfigChanges(binding: ActivityPluginBinding) {
    onAttachedToActivity(binding)
  }

  override fun onDetachedFromActivityForConfigChanges() {
    onDetachedFromActivity()
  }

  override fun onActivityResult(requestCode: Int, resultCode: Int, data: Intent?): Boolean {
    if (requestCode != PICK_FILE_REQUEST) return false
    val result = pendingPickResult
    pendingPickResult = null
    if (result == null) return true

    if (resultCode != Activity.RESULT_OK || data?.data == null) {
      result.success(null)
      return true
    }
    val uri = data.data!!
    result.success(resolveRealPath(uri))
    return true
  }
}