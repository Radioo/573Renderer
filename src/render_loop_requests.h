#pragma once

namespace App {
struct Request;
}

void DispatchAppRequest(const App::Request& req_obj);
