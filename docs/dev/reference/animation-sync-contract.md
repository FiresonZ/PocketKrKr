# 动画同步契约

## 目的

定义多实例时间线动画的通用播放、跳过、完成通知和目标隔离行为。本文只保存抽象契约，不复制任何外部实现。

## 实例所有权

- 每个动画控制器必须独立持有 motion 名称、时间、循环状态、完成状态和绘制目标。
- 捕获、绘制和跳过操作必须通过当前脚本对象或明确的实例引用定位控制器。
- 不得使用“最近播放”或“最后绘制”的全局指针作为跨实例路由依据。
- 场景切换时必须清理旧实例的目标、等待状态和临时资源。

## 时间线行为

```text
play(motion):
  reset instance clock and completion state
  load the selected timeline

progress(delta):
  advance only this instance
  evaluate loop or completion
  emit completion once for non-looping timelines

skipToSync():
  move only this instance to its synchronization boundary
  preserve the caller's completion contract
```

跳过一个动画不等于设置全局跳过模式，也不应自动修改尚未开始的后续动画。后续动画必须通过自身的 `play` 初始化时间和循环状态。

## 完成通知

- 自然结束和主动跳过都必须最多发送一次完成通知。
- 通知应绑定发出通知的动画实例。
- `stop`、销毁或场景切换后不得从旧实例继续发送通知。
- 命令列表、脚本回调和渲染目标更新必须遵守同一实例边界。

## 验收

- 同时存在两个控制器时，跳过第一个不会改变第二个的时间、循环或完成状态。
- M2logo 被跳过后，后续主界面背景动画仍从自己的起始时间开始播放。
- 连续点击、自然结束、重复播放、停止和销毁均不会重复触发完成回调。
- 捕获目标与动画实例一一对应，不依赖全局最近绘制状态。
