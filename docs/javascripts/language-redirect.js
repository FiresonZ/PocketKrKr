(() => {
  const preferenceKey = "pocketkrkr-docs-language";
  const supported = ["zh", "en"];
  const path = window.location.pathname;
  const current = path.split("/").filter(Boolean)[0];
  const browser = (navigator.language || "").toLowerCase().startsWith("en") ? "en" : "zh";

  if (supported.includes(current)) {
    try {
      localStorage.setItem(preferenceKey, current);
    } catch (_) {}
    return;
  }

  let preference = null;
  try {
    preference = localStorage.getItem(preferenceKey);
  } catch (_) {}

  if (preference || browser !== "en") {
    return;
  }

  const target = `/en${path}`.replace(/\/+/g, "/");
  if (target !== path) {
    window.location.replace(target);
  }
})();
