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

#include "inner/inner_client.h"

namespace fasto {
namespace fastotv {
namespace inner {

InnerClient::InnerClient(network::tcp::ITcpLoop* server, const common::net::socket_info& info)
    : TcpClient(server, info) {}

const char* InnerClient::ClassName() const {
  return "InnerClient";
}

common::Error InnerClient::Write(const cmd_request_t& request, ssize_t* nwrite) {
  return TcpClient::Write(request.data(), request.size(), nwrite);
}

common::Error InnerClient::Write(const cmd_responce_t& responce, ssize_t* nwrite) {
  return TcpClient::Write(responce.data(), responce.size(), nwrite);
}

common::Error InnerClient::Write(const cmd_approve_t& approve, ssize_t* nwrite) {
  return TcpClient::Write(approve.data(), approve.size(), nwrite);
}

}  // namespace inner
}  // namespace fastotv
}  // namespace fasto