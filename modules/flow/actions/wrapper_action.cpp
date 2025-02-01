/*
 *     .============.
 *    //  M A K E  / \
 *   //  C++ DEV  /   \
 *  //  E A S Y  /  \/ \
 * ++ ----------.  \/\  .
 *  \\     \     \ /\  /
 *   \\     \     \   /
 *    \\     \     \ /
 *     -============'
 *
 * Copyright (c) 2018 Hevake and contributors, all rights reserved.
 *
 * This file is part of cpp-tbox (https://github.com/cpp-main/cpp-tbox)
 * Use of this source code is governed by MIT license that can be found
 * in the LICENSE file in the root of the source tree. All contributing
 * project authors may be found in the CONTRIBUTORS.md file in the root
 * of the source tree.
 */
#include "wrapper_action.h"
#include <tbox/base/defines.h>
#include <tbox/base/assert.h>
#include <tbox/base/json.hpp>

namespace tbox {
namespace flow {

using namespace std::placeholders;

WrapperAction::WrapperAction(event::Loop &loop, Mode mode)
  : AssembleAction(loop, "Wrapper")
  , mode_(mode)
{ }

WrapperAction::WrapperAction(event::Loop &loop, Action *child, Mode mode)
  : AssembleAction(loop, "Wrapper")
  , child_(child)
  , mode_(mode)
{
    TBOX_ASSERT(child_ != nullptr);
    child_->setFinishCallback(std::bind(&WrapperAction::onChildFinished, this, _1, _2, _3));
    child_->setBlockCallback(std::bind(&WrapperAction::block, this, _1, _2));
    child_->setParent(this);
}

WrapperAction::~WrapperAction() {
    CHECK_DELETE_RESET_OBJ(child_);
}

void WrapperAction::toJson(Json &js) const {
    AssembleAction::toJson(js);
    js["mode"] = ToString(mode_);
    child_->toJson(js["child"]);
}

bool WrapperAction::setChild(Action *child) {
    CHECK_DELETE_RESET_OBJ(child_);
    child_ = child;
    if (child_ != nullptr) {
        child_->setFinishCallback(std::bind(&WrapperAction::onChildFinished, this, _1, _2, _3));
        child_->setBlockCallback(std::bind(&WrapperAction::block, this, _1, _2));
        child_->setParent(this);
    }
    return true;
}

bool WrapperAction::isReady() const {
    if (child_ == nullptr) {
        LogNotice("%d:%s[%s], no child", id(), type().c_str(), label().c_str());
        return false;
    }
    return child_->isReady();
}

void WrapperAction::onStart() {
    AssembleAction::onStart();

    TBOX_ASSERT(child_ != nullptr);
    child_->start();
}

void WrapperAction::onStop() {
    TBOX_ASSERT(child_ != nullptr);
    child_->stop();

    AssembleAction::onStop();
}

void WrapperAction::onPause() {
    TBOX_ASSERT(child_ != nullptr);
    child_->pause();

    AssembleAction::onPause();
}

void WrapperAction::onResume() {
    AssembleAction::onResume();

    if (child_->state() == State::kFinished) {
        onChildFinished(child_->result() == Result::kSuccess, Reason(), Trace());
    } else {
        child_->resume();
    }
}

void WrapperAction::onReset() {
    TBOX_ASSERT(child_ != nullptr);
    child_->reset();

    AssembleAction::onReset();
}

void WrapperAction::onChildFinished(bool is_succ, const Reason &why, const Trace &trace) {
    if (state() == State::kRunning) {
        if (mode_ == Mode::kNormal)
            finish(is_succ, why, trace);
        else if (mode_ == Mode::kInvert)
            finish(!is_succ, why, trace);
        else if (mode_ == Mode::kAlwaySucc)
            finish(true, why, trace);
        else if (mode_ == Mode::kAlwayFail)
            finish(false, why, trace);
        else
            TBOX_ASSERT(false);
    }
}

std::string ToString(WrapperAction::Mode mode) {
    const char *tbl[] = {"Normal", "Invert", "AlwaySucc", "AlwayFail"};
    auto idx = static_cast<size_t>(mode); // size_t id always >= 0
    if (idx < NUMBER_OF_ARRAY(tbl))
        return tbl[idx];
    else
        return "Unknown";
}

}
}
