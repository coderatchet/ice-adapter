#include "JsonRpcProtocol.h"

#include "Socket.h"
#include "logging.h"

namespace faf {

JsonRpcProtocol::JsonRpcProtocol():
  mCurrentId(0)
{
  FAF_LOG_TRACE << "JsonRpcProtocol()";
}

JsonRpcProtocol::~JsonRpcProtocol()
{
  FAF_LOG_TRACE << "~JsonRpcProtocol()";
}

void JsonRpcProtocol::setRpcCallback(std::string const& method,
                                      RpcCallback cb)
{
  /* We allow only one callback, because there's only one result of the RPC call */
  if (mCallbacks.find(method) == mCallbacks.end())
  {
    mCallbacks.insert(std::make_pair(method, cb));
    FAF_LOG_TRACE << "callback for " << method << " registered";
  }
  else
  {
    FAF_LOG_ERROR << "RPC callback for method '" << method << "' already registered";
  }
}

void JsonRpcProtocol::sendRequest(std::string const& method,
                                   Json::Value const& paramsArray,
                                   Socket* socket,
                                   RpcRequestResult resultCb)
{
  if (!paramsArray.isArray())
  {
    Json::Value error = "paramsArray MUST be an array";
    if (resultCb)
    {
      resultCb(Json::Value(),
               error);
    }
    return;
  }
  if (method.empty())
  {
    Json::Value error = "method MUST not be empty";
    if (resultCb)
    {
      resultCb(Json::Value(),
               error);
    }
    return;
  }

  Json::Value request;
  request["jsonrpc"] = "2.0";
  request["method"] = method;
  request["params"] = paramsArray;
  if (resultCb)
  {
    mCurrentRequests[mCurrentId] = resultCb;
    request["id"] = mCurrentId;
    ++mCurrentId;
  }
  std::string requestString = Json::FastWriter().write(request);

  if (!this->sendJson(socket, requestString))
  {
    Json::Value error = "send failed";
    if (resultCb)
    {
      resultCb(Json::Value(),
               error);
    }
  }
}

void JsonRpcProtocol::parseMessage(Socket* socket, std::vector<char>& msgBuffer)
{
  try
  {
    Json::Value jsonMessage;
    Json::Reader r;

    std::istringstream is(std::string(msgBuffer.data(), msgBuffer.size()));
    msgBuffer.clear();
    while (true)
    {
      std::string doc;
      std::getline(is, doc, '\n');
      if (doc.empty())
      {
        break;
      }
      FAF_LOG_TRACE << "parsing JSON:" << doc;
      if(!r.parse(doc, jsonMessage))
      {
        FAF_LOG_TRACE << "storing doc:" << doc;
        msgBuffer.insert(msgBuffer.end(),
                        doc.c_str(),
                        doc.c_str() + doc.size());
        break;
      }
      if (jsonMessage.isMember("method"))
      {
        /* this message is a request */
        Json::Value response = processRequest(jsonMessage, socket);

        /* we don't need to respond to notifications */
        if (jsonMessage.isMember("id"))
        {
          std::string responseString = Json::FastWriter().write(response);
          FAF_LOG_TRACE << "sending response:" << responseString;
          socket->send(responseString);
        }
      }
      else if (jsonMessage.isMember("error") ||
               jsonMessage.isMember("result"))
      {
        /* this message is a response */
        if (jsonMessage.isMember("id"))
        {
          if (jsonMessage["id"].isInt())
          {
            auto reqIt = mCurrentRequests.find(jsonMessage["id"].asInt());
            if (reqIt != mCurrentRequests.end())
            {
              reqIt->second(jsonMessage.isMember("result") ? jsonMessage["result"] : Json::Value(),
                            jsonMessage.isMember("error") ? jsonMessage["error"] : Json::Value());
              mCurrentRequests.erase(reqIt);
            }
          }
        }
      }
    }
  }
  catch (std::exception& e)
  {
    FAF_LOG_ERROR << "error in receive: " << e.what();
  }
}

Json::Value JsonRpcProtocol::processRequest(Json::Value const& request, Socket* socket)
{
  Json::Value response;
  response["jsonrpc"] = "2.0";

  if (request.isMember("id"))
  {
    response["id"] = request["id"];
  }
  if (!request.isMember("method"))
  {
    response["error"]["code"] = -1;
    response["error"]["message"] = "missing 'method' parameter";
    return response;
  }
  if (!request["method"].isString())
  {
    response["error"]["code"] = -1;
    response["error"]["message"] = "'method' parameter must be a string";
    return response;
  }

  FAF_LOG_TRACE << "dispatching JSRONRPC method '" << request["method"].asString() << "'";

  Json::Value params(Json::arrayValue);
  if (request.isMember("params") &&
      request["params"].isArray())
  {
    params = request["params"];
  }

  Json::Value result;
  Json::Value error;

  auto it = mCallbacks.find(request["method"].asString());
  if (it != mCallbacks.end())
  {
    it->second(params, result, error, socket);
  }
  else
  {
    FAF_LOG_ERROR << "RPC callback for method '" << request["method"].asString() << "' not found";
    error = std::string("RPC callback for method '") + request["method"].asString() + "' not found";
  }

  /* TODO: Better check for valid error/result combination */
  if (!result.isNull())
  {
    response["result"] = result;
  }
  else
  {
    response["error"] = error;
  }

  return response;
}


} // namespace faf
