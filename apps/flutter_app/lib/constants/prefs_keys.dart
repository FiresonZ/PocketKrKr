/// Centralized SharedPreferences keys and engine option constants.
///
/// All pages (home_page, settings_page, game_page) should reference
/// these constants instead of defining their own copies.
class PrefsKeys {
  PrefsKeys._(); // Prevent instantiation

  // ── SharedPreferences keys ──────────────────────────────────────
  static const String dylibPath = 'pocketkrkr_dylib_path';
  static const String engineMode = 'pocketkrkr_engine_mode';
  static const String perfOverlay = 'pocketkrkr_perf_overlay';
  static const String fpsLimitEnabled = 'pocketkrkr_fps_limit_enabled';
  static const String targetFps = 'pocketkrkr_target_fps';
  static const String renderer = 'pocketkrkr_renderer';
  static const String angleBackend = 'pocketkrkr_angle_backend';
  static const String locale = 'pocketkrkr_locale';
  static const String themeMode = 'pocketkrkr_theme_mode';
  static const String forceLandscape = 'pocketkrkr_force_landscape';

  /// Pending play session: JSON { "path": "...", "startTime": "ISO8601" }. Cleared on normal exit or applied on next launch.
  static const String pendingPlaySession = 'pocketkrkr_pending_play_session';

  /// Recently settled play session IDs. Used to avoid duplicate crediting.
  static const String settledPlaySessionIds = 'pocketkrkr_settled_play_session_ids';

  // ── Engine option keys (must match C++ ENGINE_OPTION_* constants) ─
  static const String optionAngleBackend = 'angle_backend';
  static const String optionFpsLimit = 'fps_limit';
  static const String optionRenderer = 'renderer';
  static const String optionMemoryProfile = 'memory_profile';
  static const String optionMemoryBudgetMb = 'memory_budget_mb';
  static const String optionMemoryLogIntervalMs = 'memory_log_interval_ms';
  static const String optionPsbCacheMb = 'psb_cache_mb';
  static const String optionPsbCacheEntries = 'psb_cache_entries';
  static const String optionArchiveCacheCount = 'archive_cache_count';
  static const String optionAutoPathCacheCount = 'autopath_cache_count';

  // ── Engine option values ────────────────────────────────────────
  static const String angleBackendGles = 'gles';
  static const String angleBackendVulkan = 'vulkan';
  static const String rendererOpengl = 'opengl';
  static const String rendererSoftware = 'software';
  static const String memoryProfileBalanced = 'balanced';
  static const String memoryProfileAggressive = 'aggressive';

  // ── Engine mode values ──────────────────────────────────────────
  static const String engineModeBuiltIn = 'builtIn';
  static const String engineModeCustom = 'custom';

  // ── Default values ──────────────────────────────────────────────
  static const int defaultFps = 60;
  static const List<int> fpsOptions = [30, 60, 120];
}
