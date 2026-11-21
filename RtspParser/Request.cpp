#include "Request.h"


namespace rtsp {

void SetBearerAuthorization(Request* request, const std::string& token)
{
#if __cplusplus > 202603L
    request->headerFields.insert_or_assign(AuthorizationFieldName, "Bearer " + token);
#else
    request->headerFields[std::string(AuthorizationFieldName)] = "Bearer " + token;
#endif
}

void SetAuthorization(Request* request, const Credentials& credentials)
{
    if(credentials.accessId.empty()) {
        SetBearerAuthorization(request, credentials.accessToken);
    } else {
#if __cplusplus > 202603L
        request->headerFields.insert_or_assign(
            AuthorizationFieldName,
            "Basic " + credentials.accessId + ":" + credentials.accessToken);
#else
        request->headerFields[std::string(AuthorizationFieldName)] =
            "Basic " + credentials.accessId + ":" + credentials.accessToken;
#endif
    }
}

}
