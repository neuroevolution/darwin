// Copyright 2018 The Darwin Neuroevolution Framework Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "game.h"
#include "player.h"

#include <core/utils.h>

#include <cmath>
#include <random>
using namespace std;

namespace pong {

Config g_config;

static constexpr float kPi = 3.14159265359f;
static constexpr float kMaxAngle = kPi / 3.0f;

void Game::newSet() {
  CHECK(player1_ != nullptr);
  CHECK(player2_ != nullptr);
  CHECK(set_ >= 0);

  ++set_;
  step_ = 0;

  paddle_pos_p1_ = 0.5f;
  paddle_pos_p2_ = 0.5f;

  ball_.x = 0;
  ball_.y = 0.5f;

  if (g_config.simple_serve) {
    ball_.vx = g_config.ball_speed;
    ball_.vy = 0;
  } else {
    random_device rd;
    default_random_engine rnd(rd());
    uniform_real_distribution<float> dist(-kMaxAngle, kMaxAngle);

    float angle = dist(rnd);
    ball_.vx = cos(angle) * g_config.ball_speed;
    ball_.vy = sin(angle) * g_config.ball_speed;
  }

  if (set_ % 2 == 1)
    ball_.vx = -ball_.vx;

  player1_->newGame(this, Player::Side::Left);
  player2_->newGame(this, Player::Side::Right);
}

void Game::reset() {
  set_ = -1;
  step_ = -1;
  max_steps_ = -1;

  score_p1_ = -1;
  score_p2_ = -1;

  player1_ = nullptr;
  player2_ = nullptr;
}

void Game::newGame(Player* player1, Player* player2) {
  set_ = 0;

  score_p1_ = 0;
  score_p2_ = 0;

  player1_ = player1;
  player2_ = player2;

  newSet();
}

static void movePaddle(float& pos, Player::Action action) {
  const float up_limit = 1 - g_config.paddle_size / 2;
  const float down_limit = g_config.paddle_size / 2;

  switch (action) {
    case Player::Action::MoveUp:
      pos += g_config.paddle_speed;
      if (pos > up_limit)
        pos = up_limit;
      break;

    case Player::Action::MoveDown:
      pos -= g_config.paddle_speed;
      if (pos < down_limit)
        pos = down_limit;
      break;

    case Player::Action::None:
      // nothing to do
      break;
  }
}

bool Game::moveBall(float dt) {
  CHECK(ball_.vx != 0);
  CHECK(dt > 0);

  enum class Contact { None, Left, Right, Up, Down };

  Contact contact = Contact::None;

  const float r = pong::g_config.ball_radius;
  const float left = -1 + pong::g_config.paddle_offset + r;
  const float right = 1 - pong::g_config.paddle_offset - r;
  const float up = 1 - r;
  const float down = r;

  float t = numeric_limits<float>::infinity();

  // left/right contacts
  if (ball_.vx > 0) {
    float tx_right = (right - ball_.x) / ball_.vx;
    if (tx_right > 0) {
      CHECK(tx_right < t);
      t = tx_right;
      contact = Contact::Right;
    }
  } else if (ball_.vx < 0) {
    float tx_left = (left - ball_.x) / ball_.vx;
    if (tx_left > 0) {
      CHECK(tx_left < t);
      t = tx_left;
      contact = Contact::Left;
    }
  }

  // up/down contacts
  if (ball_.vy > 0) {
    float tx_up = (up - ball_.y) / ball_.vy;
    if (tx_up > 0 && tx_up < t) {
      t = tx_up;
      contact = Contact::Up;
    }
  } else if (ball_.vy < 0) {
    float tx_down = (down - ball_.y) / ball_.vy;
    if (tx_down > 0 && tx_down < t) {
      t = tx_down;
      contact = Contact::Down;
    }
  }

  CHECK(contact != Contact::None);
  CHECK(t < numeric_limits<double>::infinity());

  if (t > dt) {
    ball_.x += ball_.vx * dt;
    ball_.y += ball_.vy * dt;
    return true;
  }

  ball_.x += ball_.vx * t;
  ball_.y += ball_.vy * t;

  if (contact == Contact::Up || contact == Contact::Down) {
    ball_.vy = -ball_.vy;
    return dt > t ? moveBall(dt - t) : true;
  }

  CHECK(contact == Contact::Left || contact == Contact::Right);

  const float phs = g_config.paddle_size / 2 + r;
  float dy = ball_.y - (contact == Contact::Left ? paddle_pos_p1_ : paddle_pos_p2_);

  if (fabs(dy) > phs) {
    if (contact == Contact::Left)
      ++score_p2_;
    else
      ++score_p1_;
    return false;
  }

  float angle = (dy / phs) * kMaxAngle;
  ball_.vx = cos(angle) * g_config.ball_speed;
  ball_.vy = sin(angle) * g_config.ball_speed;
  if (contact == Contact::Right)
    ball_.vx = -ball_.vx;

  return true;
}

bool Game::gameStep() {
  CHECK(step_ >= 0);

  CHECK(set_ > 0);
  CHECK(score_p1_ >= 0);
  CHECK(score_p2_ >= 0);

  if (++step_ == max_steps_) {
    // tie
    ++score_p1_;
    ++score_p2_;
    return false;
  }

  movePaddle(paddle_pos_p1_, player1_->action());
  movePaddle(paddle_pos_p2_, player2_->action());

  return moveBall(1.0f);
}

}  // namespace pong
