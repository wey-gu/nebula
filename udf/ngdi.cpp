/* Copyright (c) 2023 vesoft inc. All rights reserved.
 *
 * This source code is licensed under Apache 2.0 License.
 */

#include "ngdi.h"

#include <cmath>
#include <vector>

#include "../src/common/datatypes/List.h"
#include "../src/common/datatypes/Map.h"
#include "../src/common/http/HttpClient.h"
#include "../src/graph/service/GraphFlags.h"

DEFINE_string(ngdi_host, "http://sparkmaster:9999", "ngdi api host");

extern "C" GraphFunction *create() {
  return new ngdi;
}
extern "C" void destroy(GraphFunction *function) {
  delete function;
}

char *ngdi::name() {
  const char *name = "ngdi";
  return const_cast<char *>(name);
}

std::vector<std::vector<nebula::Value::Type>> ngdi::inputType() {
  std::vector<nebula::Value::Type> vtp1 = {nebula::Value::Type::STRING};
  std::vector<nebula::Value::Type> vtp2 = {nebula::Value::Type::LIST, nebula::Value::Type::STRING};
  std::vector<nebula::Value::Type> vtp3 = {
      nebula::Value::Type::STRING, nebula::Value::Type::LIST, nebula::Value::Type::STRING};
  std::vector<nebula::Value::Type> vtp4 = {
      nebula::Value::Type::NULLVALUE, nebula::Value::Type::STRING, nebula::Value::Type::MAP};
  std::vector<nebula::Value::Type> vtp5 = {nebula::Value::Type::NULLVALUE,
                                           nebula::Value::Type::MAP};
  std::vector<nebula::Value::Type> vtp6 = {nebula::Value::Type::NULLVALUE,
                                           nebula::Value::Type::MAP};

  std::vector<std::vector<nebula::Value::Type>> vvtp = {vtp1, vtp2, vtp3, vtp4, vtp5, vtp6};
  return vvtp;
}

nebula::Value::Type ngdi::returnType() {
  return nebula::Value::Type::MAP;
}

size_t ngdi::minArity() {
  return 1;
}

size_t ngdi::maxArity() {
  return 6;
}

bool ngdi::isPure() {
  return true;
}

nebula::Value ngdi::call_ngdi_api(
    const std::vector<std::reference_wrapper<const nebula::Value>> &args) {
  // function to make http call to ngdi-api-gateway
  // param:
  //  - read_context, Value::Type::MAP
  //  - write_context, Value::Type::MAP
  //  - algo_context, Value::Type::MAP
  //  - mode, Value::Type::STRING, default to "compact", another valid value for now is "parallel"
  // return: Value::Type::MAP

  // get the read_context
  auto read_context = args[0].get().getMap();
  // get the write_context
  auto write_context = args[1].get().getMap();
  // get the algo_context
  auto algo_context = args[2].get().getMap();
  // get the mode
  auto mode = args[3].get().getStr();
  // validate the mode value, if not valid, return response MAP with error message: "Invalid mode:
  // {mode}}"
  if (mode != "compact" && mode != "parallel") {
    nebula::Map response;
    response.kvs.emplace("error", nebula::Value("Invalid mode: " + mode));
    return nebula::Value(response);
  }

  // validate the read_mode and other read_context values, if not valid, return response MAP with
  // error message: "Invalid read_context: {read_context}}"
  auto read_mode = read_context.kvs.find("read_mode");
  // when read_mode is "query", there must be a "query" key in read_context, if not, return response
  // MAP with error message: "Invalid read_context: no query found in read_mode: query"
  if (read_mode != read_context.kvs.end() && read_mode->second.getStr() == "query") {
    auto query = read_context.kvs.find("query");
    if (query == read_context.kvs.end()) {
      nebula::Map response;
      response.kvs.emplace(
          "error", nebula::Value("Invalid read_context: no query found in read_mode: query"));
      return nebula::Value(response);
    }
    // else when read_mode is "scan", there must be "edges" and "edge_weights" keys in read_context,
    // if not, return response MAP with error message: "Invalid read_context: no edges or
    // edge_weights found in read_mode: scan"
  } else if (read_mode != read_context.kvs.end() && read_mode->second.getStr() == "scan") {
    auto edges = read_context.kvs.find("edges");
    auto edge_weights = read_context.kvs.find("edge_weights");
    if (edges == read_context.kvs.end() || edge_weights == read_context.kvs.end()) {
      nebula::Map response;
      response.kvs.emplace(
          "error",
          nebula::Value("Invalid read_context: no edges or edge_weights found in read_mode: scan"));
      return nebula::Value(response);
    }
    // else, other invalid read_mode values, return response MAP with error message: "Invalid
    // read_context: {read_context}}"
  } else {
    nebula::Map response;
    response.kvs.emplace("error", nebula::Value("Invalid read_context: " + read_context));
    return nebula::Value(response);
  }
  // validate name exists in algo_context, and the value is in ["label_propagation", "louvain",
  // "k_core", "degree_statics", "betweenness_centrality", "coefficient_centrality", "bfs", "hanp",
  // "jaccard", "strong_connected_components", "triangle_coun"] if not valid, return response MAP
  // with error message: "Invalid algo_name: {algo_name}"
  auto algo_name = algo_context.kvs.find("algo_name");
  if (algo_name == algo_context.kvs.end()) {
    nebula::Map response;
    response.kvs.emplace("error", nebula::Value("Invalid algo_context: no algo_name found"));
    return nebula::Value(response);
  }
  auto algo_name_value = algo_name->second.getStr();
  if (algo_name_value != "label_propagation" && algo_name_value != "louvain" &&
      algo_name_value != "k_core" && algo_name_value != "degree_statics" &&
      algo_name_value != "betweenness_centrality" && algo_name_value != "coefficient_centrality" &&
      algo_name_value != "bfs" && algo_name_value != "hanp" && algo_name_value != "jaccard" &&
      algo_name_value != "strong_connected_components" && algo_name_value != "triangle_coun") {
    nebula::Map response;
    response.kvs.emplace("error", nebula::Value("Invalid algo_name: " + algo_name_value));
    return nebula::Value(response);
  }
  // validate the config in algo_context when there is a config key in algo_context, it should be a
  // MAP
  auto config = algo_context.kvs.find("config");
  if (config != algo_context.kvs.end() && config->second.type() != nebula::Value::Type::MAP) {
    nebula::Map response;
    response.kvs.emplace("error", nebula::Value("Invalid algo_context: config should be a MAP"));
    return nebula::Value(response);
  }

  // api gateway url prefix should be like: "http://sparkmaster:9999/api/v0/"
  // build url prefix from ngdi_host
  auto ngdi_host = FLAGS_ngdi_host;
  std::string url_prefix = ngdi_host + "/api/v0/";
  // call the ngdi api gateway with the url_prefix on /api/v0/{mode}/{algo_name}

  // the body is {"read_context": {read_context}, "write_context": {write_context}, "algo_context":
  // {algo_context}} build the body
  nebula::Map body;
  body.kvs.emplace("read_context", nebula::Value(read_context));
  body.kvs.emplace("write_context", nebula::Value(write_context));
  body.kvs.emplace("algo_context", nebula::Value(algo_context));
  // build the string body
  auto body_json = body.toJson();
  std::string body_str = folly::toJson(body_json);

  // build the headers
  std::vector<std::string> headers;
  // set the content-type to application/json
  headers.emplace_back("Content-Type: application/json");

  // make the http request, from HttpClient
  auto response_str = nebula::HttpClient::instance().post(
      url_prefix + mode + "/" + algo_name_value, headers, body_str);
  // build response MAP from response_str and return
  nebula::Map response;
  response.kvs.emplace("response", nebula::Value(response_str.body));
  return nebula::Value(response);
}

  nebula::Value ngdi::body(const std::vector<std::reference_wrapper<const nebula::Value>> &args) {
    // context MAPs to be passed to ngdi api gateway
    nebula::Map read_context;
    nebula::Map write_context;
    nebula::Map algo_context;
    nebula::Value mode = nebula::Value("compact");

    // validate the args size, if smaller than 2, return response MAP with error message: "Invalid
    // args size: {args.size()}"
    if (args.size() < 2) {
      nebula::Map response;
      response.kvs.emplace("error",
                           nebula::Value("Invalid args size: " + std::to_string(args.size())));
      return nebula::Value(response);
    } else if (args[0].get().type() != nebula::Value::Type::STRING) {
      // validate the first arg is a string, if not, return response MAP with error message:
      // "Invalid args[0]: {args[0]}"
      nebula::Map response;
      response.kvs.emplace("error", nebula::Value("Invalid args[0]: " + args[0].get()));
      return nebula::Value(response);
    } else if (args[1].get().type() == nebula::Value::Type::LIST) {
      // ----------------------------------------------------------
      // ngdi_algo("pagerank", ["follow"], ["degree"], "parallel") # default algo conf and write
      // conf ngdi_algo("pagerank", ["follow"], ["degree"], "parallel", {max_iter: 10}, {write_mode:
      // "insert"})
      // ----------------------------------------------------------
      // if the second arg is a LIST, its read_mode is "scan"
      // validate:
      // 0. the size of the second arg is larger than 0
      // 1. there are third arg
      // 2. the third arg is a LIST
      // 3. the size of the second arg and the third arg are the same all the elements are STRING
      // if not, return response MAP with error message: "Invalid args[1]: edge_types and
      // edge_weights should be a LIST of STRING in same size"

      if (args[1].get().isList() && args[1].get().getList().size() == 0) {
        nebula::Map response;
        response.kvs.emplace(
            "error",
            nebula::Value("Invalid args[1]: edge_types should be a LIST of STRING in same size"));
        return nebula::Value(response);
      }
      if (args.size() < 3) {
        nebula::Map response;
        response.kvs.emplace("error",
                             nebula::Value("Invalid args size: " + std::to_string(args.size())));
        return nebula::Value(response);
      }
      if (args[2].get().type() != nebula::Value::Type::LIST) {
        nebula::Map response;
        response.kvs.emplace(
            "error",
            nebula::Value("Invalid args[2]: edge_weights should be a LIST of STRING in same size"));
        return nebula::Value(response);
      }

      // args[1] and args[2] should be List
      if (!args[1].get().isList() || !args[2].get().isList()) {
        nebula::Map response;
        response.kvs.emplace(
            "error",
            nebula::Value("Invalid args[1]: edge_types and edge_weights should be a "
                          "LIST of STRING in same size"));
        return nebula::Value(response);
      }
      if (args[1].get().getList().size() != args[2].get().getList().size()) {
        nebula::Map response;
        response.kvs.emplace(
            "error",
            nebula::Value("Invalid args[1]: edge_types and edge_weights should be a "
                          "LIST of STRING in same size"));
        return nebula::Value(response);
      }
      for (auto i = 0; i < args[1].get().getList().size(); i++) {
        if (args[1].get().getList()[i].type() != nebula::Value::Type::STRING ||
            args[2].get().getList()[i].type() != nebula::Value::Type::STRING) {
          nebula::Map response;
          response.kvs.emplace(
              "error",
              nebula::Value("Invalid args[1]: edge_types and edge_weights should be a "
                            "LIST of STRING in same size"));
          return nebula::Value(response);
        }
      }

      // build the read_context
      read_context.kvs.emplace("read_mode", nebula::Value("scan"));
      read_context.kvs.emplace("edge_types", nebula::Value(args[1].get()));
      read_context.kvs.emplace("edge_weights", nebula::Value(args[2].get()));

      // build mode from the 4th arg if there is one, otherwise use nebula::Value("compact"))

      if (args.size() >= 4) {
        mode = args[3].get();
      }
      // build the algo_context

      if (!args[4].get().isMap()) {
        nebula::Map response;
        response.kvs.emplace("error", nebula::Value("Invalid algo_context: should be a MAP"));
        return nebula::Value(response);
      }

      if (args.size() >= 5) {
        algo_context = args[4].get().getMap();
      } else {
        algo_context = nebula::Map();
      }

      // add the name to algo_context
      algo_context.kvs.emplace("name", nebula::Value(args[0].get().getStr()));

      // build the write_context
      nebula::Map write_context;

      if (args.size() >= 6) {
        write_context = args[5].get().getMap();
      } else {
        write_context = nebula::Map();
      }

      // validate that if algo_context is not empty, it should be a MAP
      if (write_context.size() > 0) {
        nebula::Map response;
        response.kvs.emplace("error", nebula::Value("Invalid write_context: should be a MAP"));
        return nebula::Value(response);
      }
      // add the algo_name to write_context
      write_context.kvs.emplace("algo_name", nebula::Value(args[0].get().getStr()));

    } else if (args[1].get().type() == nebula::Value::Type::STRING) {
      // ----------------------------------------------------------
      // ngdi_algo("pagerank", $-.query, "parallel") # default algo conf and write conf
      // ngdi_algo("pagerank", $-.query, "parallel", {max_iter: 10}, {write_mode: "insert"})
      // ----------------------------------------------------------
      // if the second arg is a STRING, its read_mode is "query"
      // validate it's not empty
      // TBD(wey): need to add edges and edge_weights to query read_mode, too, the edge_weights
      // could have None as item, too.

      if (args[1].get().getStr().empty()) {
        nebula::Map response;
        response.kvs.emplace("error", nebula::Value("Invalid args[1]: query should be a STRING"));
        return nebula::Value(response);
      }

      // build the read_context
      read_context.kvs.emplace("read_mode", nebula::Value("query"));
      read_context.kvs.emplace("query", nebula::Value(args[1].get()));

      // build mode from the 3th arg if there is one, otherwise use nebula::Value("compact"))
      if (args.size() >= 3) {
        mode = args[3].get();
      }
      // build the algo_context
      if (args.size() >= 4) {
        algo_context = args[3].get().getMap();
      } else {
        algo_context = nebula::Map();
      }
      // validate that if algo_context is not empty, it should be a MAP
      if (algo_context.size() > 0) {
        nebula::Map response;
        response.kvs.emplace("error", nebula::Value("Invalid algo_context: should be a MAP"));
        return nebula::Value(response);
      }
      // add the name to algo_context
      algo_context.kvs.emplace("name", nebula::Value(args[0].get().getStr()));

      // build the write_context
      nebula::Map write_context;

      if (args.size() >= 5) {
        write_context = args[4].get().getMap();
      } else {
        write_context = nebula::Map();
      }

      // validate that if algo_context is not empty, it should be a MAP
      if (write_context.size() > 0) {
        nebula::Map response;
        response.kvs.emplace("error", nebula::Value("Invalid write_context: should be a MAP"));
        return nebula::Value(response);
      }
      // add the algo_name to write_context
      write_context.kvs.emplace("algo_name", nebula::Value(args[0].get().getStr()));
    } else {
      // not supported args pattern
      nebula::Map response;
      response.kvs.emplace("error", nebula::Value("Invalid args[1]: should be a LIST or STRING"));
      return nebula::Value(response);
    }

    std::vector<nebula::Value> api_args;
    api_args.emplace_back(nebula::Value(read_context));
    api_args.emplace_back(nebula::Value(write_context));
    api_args.emplace_back(nebula::Value(algo_context));
    api_args.emplace_back(nebula::Value("mode"));

    std::vector<std::reference_wrapper<const nebula::Value>> ref_api_args;
    for (const auto &arg : api_args) {
      ref_api_args.emplace_back(std::cref(arg));
    }

    auto response = call_ngdi_api(ref_api_args);

    // return the response
    return nebula::Value(response);
  }
