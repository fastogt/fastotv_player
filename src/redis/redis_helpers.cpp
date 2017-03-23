/*  Copyright (C) 2014-2017 FastoGT. All right reserved.

    This file is part of FastoTV.

    FastoTV is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    FastoTV is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with FastoTV. If not, see <http://www.gnu.org/licenses/>.
*/

#include "redis/redis_helpers.h"

#include <hiredis/hiredis.h>

#include <string>
#include <vector>

#include <common/utils.h>
#include <common/logger.h>

#include "third-party/json-c/json-c/json.h"

#define GET_ALL_USERS_REDIS_REQUEST "HGETALL users"
#define GET_USER_1E "HGET users %s"

#undef ERROR

namespace fasto {
namespace fastotv {
namespace server {
namespace {

redisContext* redis_connect(const redis_configuration_t& config) {
  const common::net::HostAndPort redisHost = config.redis_host;
  const std::string unixPath = config.redis_unix_socket;

  if (!redisHost.isValid() && unixPath.empty()) {
    return NULL;
  }

  struct redisContext* redis = NULL;
  if (unixPath.empty()) {
    redis = redisConnect(redisHost.host.c_str(), redisHost.port);
  } else {
    redis = redisConnectUnix(unixPath.c_str());
    if (!redis || redis->err) {
      if (redis) {
        ERROR_LOG() << "REDIS UNIX CONNECTION ERROR: " << redis->errstr;
        redisFree(redis);
        redis = NULL;
      }
      redis = redisConnect(redisHost.host.c_str(), redisHost.port);
    }
  }

  if (redis->err) {
    ERROR_LOG() << "REDIS CONNECTION ERROR: " << redis->errstr;
    redisFree(redis);
    return NULL;
  }

  return redis;
}

common::Error parse_user_json(const char* userJson, user_id_t* out_uid, UserInfo* out_info) {
  if (!userJson || !out_uid || !out_info) {
    return common::make_error_value("Invalid input argument(s)", common::ErrorValue::E_ERROR);
  }

  json_object* obj = json_tokener_parse(userJson);
  if (!obj) {
    return common::make_error_value("Can't parse database field", common::ErrorValue::E_ERROR);
  }

  json_object* jid = NULL;
  json_bool jid_exists = json_object_object_get_ex(obj, "id", &jid); // mongodb id
  if (!jid_exists) {
    json_object_put(obj);
    return common::make_error_value("Can't parse database field", common::ErrorValue::E_ERROR);
  }

  *out_uid = json_object_get_string(jid);
  *out_info = UserInfo::MakeClass(obj);
  json_object_put(obj);
  return common::Error();
}

}  // namespace

RedisStorage::RedisStorage() : config_() {}

void RedisStorage::SetConfig(const redis_configuration_t& config) {
  config_ = config;
}

common::Error RedisStorage::FindUserAuth(const AuthInfo& user, user_id_t* uid) const {
  UserInfo uinf;
  return FindUser(user, uid, &uinf);
}

common::Error RedisStorage::FindUser(const AuthInfo& user, user_id_t* uid, UserInfo* uinf) const {
  if (!user.IsValid() || !uid || !uinf) {
    return common::make_error_value("Invalid input argument(s)", common::ErrorValue::E_ERROR);
  }

  redisContext* redis = redis_connect(config_);
  if (!redis) {
    return common::make_error_value("Can't connect to user database", common::ErrorValue::E_ERROR);
  }

  std::string login = user.login;
  const char* login_str = login.c_str();
  redisReply* reply = reinterpret_cast<redisReply*>(redisCommand(redis, GET_USER_1E, login_str));
  if (!reply) {
    redisFree(redis);
    return common::make_error_value("User not found", common::ErrorValue::E_ERROR);
  }

  const char* userJson = reply->str;
  UserInfo linfo;
  user_id_t luid;
  common::Error err = parse_user_json(userJson, &luid, &linfo);
  if (err && err->isError()) {
    freeReplyObject(reply);
    redisFree(redis);
    return err;
  }

  if (user.password != linfo.GetPassword()) {
    freeReplyObject(reply);
    redisFree(redis);
    return common::make_error_value("Password missmatch", common::ErrorValue::E_ERROR);
  }

  *uid = luid;
  *uinf = linfo;
  freeReplyObject(reply);
  redisFree(redis);
  return common::Error();
}

RedisSubHandler::~RedisSubHandler() {}

RedisSub::RedisSub(RedisSubHandler* handler) : handler_(handler), stop_(false) {}

void RedisSub::SetConfig(const redis_sub_configuration_t& config) {
  config_ = config;
}

void RedisSub::Listen() {
  redisContext* redis_sub = redis_connect(config_);
  if (!redis_sub) {
    return;
  }

  const char* channel_str = config_.channel_in.c_str();

  void* reply = redisCommand(redis_sub, "SUBSCRIBE %s", channel_str);
  if (!reply) {
    redisFree(redis_sub);
    return;
  }

  while (!stop_) {
    redisReply* lreply = NULL;
    void** plreply = reinterpret_cast<void**>(&lreply);
    if (redisGetReply(redis_sub, plreply) != REDIS_OK) {
      WARNING_LOG() << "REDIS PUB/SUB GET REPLY ERROR: " << redis_sub->errstr;
      break;
    }

    bool is_error_reply = lreply->type != REDIS_REPLY_ARRAY || lreply->elements != 3 ||
                          lreply->element[1]->type != REDIS_REPLY_STRING ||
                          lreply->element[2]->type != REDIS_REPLY_STRING;
    UNUSED(is_error_reply);

    char* chn = lreply->element[1]->str;
    size_t chn_len = lreply->element[1]->len;
    char* msg = lreply->element[2]->str;
    size_t msg_len = lreply->element[2]->len;

    if (handler_) {
      handler_->handleMessage(std::string(chn, chn_len), std::string(msg, msg_len));
    }

    freeReplyObject(lreply);
  }

  freeReplyObject(reply);
  redisFree(redis_sub);
}

void RedisSub::Stop() {
  stop_ = true;
}

bool RedisSub::PublishStateToChannel(const std::string& msg) {
  const char* channel = common::utils::c_strornull(config_.channel_clients_state);
  size_t chn_len = config_.channel_clients_state.length();
  return Publish(channel, chn_len, msg.c_str(), msg.length());
}

bool RedisSub::PublishToChannelOut(const std::string& msg) {
  const char* channel = common::utils::c_strornull(config_.channel_out);
  size_t chn_len = config_.channel_out.length();
  return Publish(channel, chn_len, msg.c_str(), msg.length());
}

bool RedisSub::Publish(const char* chn, size_t chn_len, const char* msg, size_t msg_len) {
  if (!chn || chn_len == 0) {
    return false;
  }

  if (!msg || msg_len == 0) {
    return false;
  }

  redisContext* redis_sub = redis_connect(config_);
  if (!redis_sub) {
    return false;
  }

  void* rreply = redisCommand(redis_sub, "PUBLISH %s %s", chn, msg);
  if (!rreply) {
    redisFree(redis_sub);
    return false;
  }

  freeReplyObject(rreply);
  redisFree(redis_sub);
  return true;
}

}  // namespace server
}  // namespace fastotv
}  // namespace fasto