#include "timer.h"

#include <sys/time.h>
#include <cassert>

#include <tbox/base/log.h>
#include <tbox/event/loop.h>
#include <tbox/event/timer_event.h>

namespace tbox {
namespace timer {

Timer::Timer(event::Loop *wp_loop) :
  wp_loop_(wp_loop),
  sp_timer_ev_(wp_loop->newTimerEvent())
{
  sp_timer_ev_->setCallback([this] { onTimeExpired(); });
}

Timer::~Timer() {
  assert(cb_level_ == 0); //!< 防止回调中析构

  cleanup();
  delete sp_timer_ev_;
}

void Timer::setTimezone(int offset_minutes) {
  timezone_offset_seconds_ = offset_minutes * 60;
  using_independ_timezone_ = true;
}

bool Timer::isEnabled() const {
  return state_ == State::kRunning;
}

bool Timer::enable() {
  if (state_ == State::kInited) {
    if (onEnable())
      return activeTimer();
  }

  LogWarn("should initialize first");
  return false;
}

bool Timer::disable() {
  if (state_ == State::kRunning) {
    if (onDisable()) {
      state_ = State::kInited;
      return sp_timer_ev_->disable();
    }
  }
  return false;
}

void Timer::cleanup() {
  if (state_ < State::kInited)
    return;

  onCleanup();
  disable();

  cb_ = nullptr;
  using_independ_timezone_ = false;
  timezone_offset_seconds_ = 0;
  state_ = State::kNone;
}

namespace {
//! 获取系统的时区偏移秒数
int GetSystemTimezoneOffsetSeconds() {
  //! 假设当前0时区的时间是 1970-1-1 12:00，即 utc_ts = 12 * 3600
  //! 通过 localtime_r() 获取本地的时间 local_tm。
  //! 通过 local_tm 中的 hour, min, sec 可计算出本地的时间戳 local_ts。
  //! 再用本地的时间戳减去 utc_ts 即可得出期望的值。
  //
  //! 为什么选用0时区的12时，而不是其它时间点呢？
  //! 因为在这个时间点上，计算出的任何一个时间的时间都是在 00:00 ~ 23:59 之间的
  struct tm local_tm;
  time_t utc_ts = 12 * 3600;
  localtime_r(&utc_ts, &local_tm);
  int local_ts = local_tm.tm_hour * 3600 + local_tm.tm_min * 60 + local_tm.tm_sec;
  return (local_ts - static_cast<int>(utc_ts));
}
}

bool Timer::activeTimer() {
  //! 使用 gettimeofday() 获取当前0时区的时间戳，精确到微秒
  struct timeval tv;
  int ret = gettimeofday(&tv, nullptr);
  if (ret != 0) {
    LogWarn("gettimeofday() fail, ret:%d", ret);
    return false;
  }

  int timezone_offset_seconds = using_independ_timezone_ ? \
                                timezone_offset_seconds_ : GetSystemTimezoneOffsetSeconds();
  //! 计算需要等待的秒数
  auto wait_seconds = calculateWaitSeconds(tv.tv_sec + timezone_offset_seconds);
#if 1
  LogTrace("wait_seconds:%d", wait_seconds);
#endif
  if (wait_seconds < 0)
    return false;

  //! 提升精度，计算中需要等待的毫秒数
  auto wait_milliseconds = wait_seconds * 1000 - tv.tv_usec / 1000;
  //! 启动定时器
  sp_timer_ev_->initialize(std::chrono::milliseconds(wait_milliseconds), event::Event::Mode::kOneshot);
  sp_timer_ev_->enable();

  state_ = State::kRunning;
  return true;
}

void Timer::onTimeExpired() {
  state_ = State::kInited;
  activeTimer();

  ++cb_level_;
  if (cb_)
    cb_();
  --cb_level_;
}

}
}
