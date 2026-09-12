# Download

PocketKrKr development builds are packaged automatically by [GitHub Actions](https://github.com/FiresonZ/PocketKrKr/actions), and releases are hosted on GitHub **Releases**. The download links below retrieve the latest Android and iOS Releases respectively, and update automatically when new builds are published.

> **[View All Releases](https://github.com/FiresonZ/PocketKrKr/releases)**

## Android

Install the APK directly (no signature required).

<div id="android-download" markdown>
Fetching the latest Android build…
</div>

- **System requirements**: Android 7.0 / API 24+　·　arm64-v8a　·　GPU with Vulkan support required
- **Installation**: After downloading the APK, allow “Install unknown apps”; updates can be installed directly over the old package.

## iOS

Unsigned `.ipa`; install it after signing and sideloading it with your own Apple developer certificate/tools.

<div id="ios-download" markdown>
Fetching the latest iOS build…
</div>

- **System requirements**: iOS / iPadOS 15.0+　·　arm64
- **Installation**: This package is an **unsigned** `.ipa`; sign it yourself before sideloading. You can use
  [AltStore](https://altstore.io) / Sideloadly (with an Apple ID and “Trust Developer”).

## Updates and Feedback

- Notes and artifacts for subsequent versions are published in [Releases](https://github.com/FiresonZ/PocketKrKr/releases) under “Latest”.
- Submit usage issues or crash reports to [Issues](https://github.com/FiresonZ/PocketKrKr/issues);
  see [Supported Games](support_games.md) for compatibility information.

<script>
(() => {
  const repository = "FiresonZ/PocketKrKr";
  const releasesUrl = `https://api.github.com/repos/${repository}/releases?per_page=100`;
  const builds = [
    {
      id: "android-download",
      platform: "Android",
      tagPrefix: "android-v",
      assetName: "PocketKrKr-Android-release.apk",
      releaseLabel: "PocketKrKr Android"
    },
    {
      id: "ios-download",
      platform: "iOS",
      tagPrefix: "ios-v",
      assetName: "PocketKrKr-iOS-release-nosign.ipa",
      releaseLabel: "PocketKrKr iOS"
    }
  ];

  const escapeHtml = (value) => value.replace(/[&<>'"]/g, (character) => ({
    "&": "&amp;",
    "<": "&lt;",
    ">": "&gt;",
    "'": "&#39;",
    "\"": "&quot;"
  }[character]));

  const formatDate = (value) => new Intl.DateTimeFormat("en-US", {
    dateStyle: "medium"
  }).format(new Date(value));

  const renderError = (build, message) => {
    document.getElementById(build.id).innerHTML = `<p>Unable to fetch the latest ${build.platform} build: ${escapeHtml(message)}. Download it from the <a href="https://github.com/${repository}/releases">Releases</a> page.</p>`;
  };

  const renderBuild = (build, releases) => {
    const release = releases
      .filter((item) => !item.draft && item.tag_name.startsWith(build.tagPrefix))
      .sort((left, right) => new Date(right.published_at || right.created_at) - new Date(left.published_at || left.created_at))
      .find((item) => item.assets.some((asset) => asset.name === build.assetName));

    if (!release) {
      renderError(build, "No downloadable build was found");
      return;
    }

    const asset = release.assets.find((item) => item.name === build.assetName);
    const version = release.tag_name.slice(build.tagPrefix.length);
    const size = asset.size ? `(${(asset.size / 1024 / 1024).toFixed(1)} MB)` : "";
    document.getElementById(build.id).innerHTML = `
      <ul>
        <li><strong>Latest version</strong>: <a href="${release.html_url}">${build.releaseLabel} v${escapeHtml(version)}</a> (${formatDate(release.published_at || release.created_at)})</li>
        <li><strong>Direct download</strong>: <a href="${asset.browser_download_url}">${escapeHtml(asset.name)}</a> ${size}</li>
      </ul>`;
  };

  fetch(releasesUrl, { headers: { Accept: "application/vnd.github+json" } })
    .then((response) => {
      if (!response.ok) throw new Error(`GitHub API returned ${response.status}`);
      return response.json();
    })
    .then((releases) => builds.forEach((build) => renderBuild(build, releases)))
    .catch((error) => builds.forEach((build) => renderError(build, error.message)));
})();
</script>
