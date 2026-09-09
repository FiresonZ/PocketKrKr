# 下载

PocketKrKr 的开发构建由 [GitHub Actions](https://github.com/FiresonZ/PocketKrKr/actions) 自动打包，发布物托管在 GitHub **Releases**。下面的下载链接会分别读取最新的 Android 和 iOS Release，发布新构建后页面会自动更新。

> **[查看全部 Release](https://github.com/FiresonZ/PocketKrKr/releases)**

## Android

直接安装 APK（无需签名）。

<div id="android-download" markdown>
正在获取最新 Android 构建……
</div>

- **系统要求**：Android 7.0 / API 24+　·　arm64-v8a　·　需支持 Vulkan 的 GPU
- **安装方式**：下载 APK 后允许「安装未知来源应用」即可；更新时直接覆盖安装旧包。

## iOS

未签名 `.ipa`，需用你自己的 Apple 开发者证书/工具侧载签名后安装。

<div id="ios-download" markdown>
正在获取最新 iOS 构建……
</div>

- **系统要求**：iOS / iPadOS 15.0+　·　arm64
- **安装方式**：本包为**未签名** `.ipa`，需自行签名后侧载；可用
  [AltStore](https://altstore.io) / Sideloadly（配合 Apple ID 与「信任开发者」）。

## 更新与反馈

- 后续版本的说明与产物均发布在 [Releases](https://github.com/FiresonZ/PocketKrKr/releases)「最新」。
- 使用问题/崩溃反馈请到 [Issues](https://github.com/FiresonZ/PocketKrKr/issues) 提交；
  兼容性情况见 [支持的游戏](support_games.md)。

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

  const formatDate = (value) => new Intl.DateTimeFormat("zh-CN", {
    dateStyle: "medium"
  }).format(new Date(value));

  const renderError = (build, message) => {
    document.getElementById(build.id).innerHTML = `<p>暂时无法获取最新 ${build.platform} 构建：${escapeHtml(message)}。请前往 <a href="https://github.com/${repository}/releases">Releases</a> 页面下载。</p>`;
  };

  const renderBuild = (build, releases) => {
    const release = releases
      .filter((item) => !item.draft && item.tag_name.startsWith(build.tagPrefix))
      .sort((left, right) => new Date(right.published_at || right.created_at) - new Date(left.published_at || left.created_at))
      .find((item) => item.assets.some((asset) => asset.name === build.assetName));

    if (!release) {
      renderError(build, "尚未找到可下载的构建");
      return;
    }

    const asset = release.assets.find((item) => item.name === build.assetName);
    const version = release.tag_name.slice(build.tagPrefix.length);
    const size = asset.size ? `（${(asset.size / 1024 / 1024).toFixed(1)} MB）` : "";
    document.getElementById(build.id).innerHTML = `
      <ul>
        <li><strong>最新版本</strong>：<a href="${release.html_url}">${build.releaseLabel} v${escapeHtml(version)}</a>（${formatDate(release.published_at || release.created_at)}）</li>
        <li><strong>直接下载</strong>：<a href="${asset.browser_download_url}">${escapeHtml(asset.name)}</a> ${size}</li>
      </ul>`;
  };

  fetch(releasesUrl, { headers: { Accept: "application/vnd.github+json" } })
    .then((response) => {
      if (!response.ok) throw new Error(`GitHub API 返回 ${response.status}`);
      return response.json();
    })
    .then((releases) => builds.forEach((build) => renderBuild(build, releases)))
    .catch((error) => builds.forEach((build) => renderError(build, error.message)));
})();
</script>
