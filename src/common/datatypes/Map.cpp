/* Copyright (c) 2020 vesoft inc. All rights reserved.
 *
 * This source code is licensed under Apache 2.0 License.
 */

#include "common/datatypes/Map.h"

#include <folly/String.h>
#include <folly/json.h>

#include <sstream>

#include "common/datatypes/List.h"

namespace nebula {

std::string Map::toString() const {
  std::vector<std::string> value(kvs.size());
  std::transform(kvs.begin(), kvs.end(), value.begin(), [](const auto& iter) -> std::string {
    std::stringstream out;
    out << iter.first << ":" << iter.second;
    return out.str();
  });

  std::stringstream os;
  os << "{" << folly::join(",", value) << "}";
  return os.str();
}

folly::dynamic Map::toJson() const {
  folly::dynamic mapJsonObj = folly::dynamic::object();

  for (const auto& iter : kvs) {
    mapJsonObj.insert(iter.first, iter.second.toJson());
  }

  return mapJsonObj;
}

folly::dynamic Map::getMetaData() const {
  auto mapMetadataObj = folly::dynamic::array();

  for (const auto& kv : kvs) {
    mapMetadataObj.push_back(kv.second.getMetaData());
  }

  return mapMetadataObj;
}

// Map constructor to covert from folly::dynamic object
// Called by function: json_extract()

// TODO(wey-gu) support Datetime
Map::Map(const folly::dynamic& obj) {
  DCHECK(obj.isObject());
  for (auto& kv : obj.items()) {
    switch (kv.second.type()) {
      case folly::dynamic::Type::STRING:
        kvs.emplace(kv.first.asString(), Value(kv.second.asString()));
        break;
      case folly::dynamic::Type::INT64:
        kvs.emplace(kv.first.asString(), Value(kv.second.asInt()));
        break;
      case folly::dynamic::Type::DOUBLE:
        kvs.emplace(kv.first.asString(), Value(kv.second.asDouble()));
        break;
      case folly::dynamic::Type::BOOL:
        kvs.emplace(kv.first.asString(), Value(kv.second.asBool()));
        break;
      case folly::dynamic::Type::NULLT:
        kvs.emplace(kv.first.asString(), Value());
        break;
      case folly::dynamic::Type::OBJECT:
        kvs.emplace(kv.first.asString(), Value(Map(kv.second)));
        break;
      case folly::dynamic::Type::ARRAY: {
        std::vector<Value> values;
        for (auto& item : kv.second) {
          switch (item.type()) {
            case folly::dynamic::Type::STRING:
              values.emplace_back(Value(item.asString()));
              break;
            case folly::dynamic::Type::INT64:
              values.emplace_back(Value(item.asInt()));
              break;
            case folly::dynamic::Type::DOUBLE:
              values.emplace_back(Value(item.asDouble()));
              break;
            case folly::dynamic::Type::BOOL:
              values.emplace_back(Value(item.asBool()));
              break;
            case folly::dynamic::Type::NULLT:
              values.emplace_back(Value());
              break;
            case folly::dynamic::Type::OBJECT:
              values.emplace_back(Value(Map(item)));
              break;
            case folly::dynamic::Type::ARRAY:
              // item is an array, we don't support nested array
              LOG(WARNING) << "JSON_EXTRACT: Nested array is not supported";
            default:
              LOG(WARNING) << "JSON_EXTRACT: Unsupported value type: " << item.typeName();
              break;
          }
        }
        kvs.emplace(kv.first.asString(), Value(List(std::move(values))));
      } break;
      default:
        LOG(WARNING) << "JSON_EXTRACT: Unsupported value type: " << kv.second.typeName();
        break;
    }
  }
}

}  // namespace nebula

namespace std {
std::size_t hash<nebula::Map>::operator()(const nebula::Map& m) const {
  size_t seed = 0;
  for (auto& v : m.kvs) {
    seed ^= hash<std::string>()(v.first) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    seed ^= hash<nebula::Value>()(v.second) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
  }
  return seed;
}

}  // namespace std
